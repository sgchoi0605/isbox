#include "MemberController.h"

// 세션 쿠키 헤더 문자열 조립(std::ostringstream)을 위해 포함한다.
#include <sstream>
// std::move를 사용해 콜백 소유권을 전달하기 위해 포함한다.
#include <utility>

namespace
{

// 브라우저에 저장되는 로그인 세션 쿠키 이름이다.
constexpr const char *kSessionCookieName = "isbox_session";

// Controller 단계에서 바로 반환해야 하는 오류 응답의 공통 JSON 형태를 만든다.
Json::Value makeErrorBody(const std::string &message)
{
    // 오류 응답용 JSON 객체를 생성한다.
    Json::Value body;
    // 성공 여부를 명시적으로 false로 고정한다.
    body["ok"] = false;
    // 호출자에게 전달할 오류 메시지를 설정한다.
    body["message"] = message;
    // 완성된 오류 응답 본문을 반환한다.
    return body;
}

}  // namespace

namespace auth
{

void MemberController::registerHandlers()
{
    // 전역 Drogon 앱 인스턴스를 참조한다.
    auto &app = drogon::app();

    // 회원가입은 JSON body를 받아 새 회원을 만들고 즉시 세션 쿠키를 발급한다.
    app.registerHandler(
        "/api/auth/signup",
        [this](const drogon::HttpRequestPtr &request, Callback &&callback) {
            handleSignup(request, std::move(callback));
        },
        {drogon::Post});

    // 로그인은 이메일/비밀번호를 확인하고 성공 시 세션 쿠키를 발급한다.
    app.registerHandler(
        "/api/auth/login",
        [this](const drogon::HttpRequestPtr &request, Callback &&callback) {
            handleLogin(request, std::move(callback));
        },
        {drogon::Post});

    // 현재 쿠키의 세션 토큰이 유효한지 확인하고 회원 정보를 반환한다.
    app.registerHandler(
        "/api/members/me",
        [this](const drogon::HttpRequestPtr &request, Callback &&callback) {
            handleMe(request, std::move(callback));
        },
        {drogon::Get});

    // 내 프로필 조회 엔드포인트를 GET /api/members/profile에 연결한다.
    app.registerHandler(
        "/api/members/profile",
        [this](const drogon::HttpRequestPtr &request, Callback &&callback) {
            handleGetMyProfile(request, std::move(callback));
        },
        {drogon::Get});

    // 다른 회원 프로필 조회 엔드포인트를 GET /api/members/{memberId}/profile에 연결한다.
    // {1}은 첫 번째 경로 파라미터를 의미하며 문자열로 전달된다.
    app.registerHandler(
        "/api/members/{1}/profile",
        [this](const drogon::HttpRequestPtr &request,
               Callback &&callback,
               const std::string &memberIdValue) {
            handleGetMemberProfile(request, std::move(callback), memberIdValue);
        },
        {drogon::Get});

    // 내 프로필 수정 엔드포인트를 PUT /api/members/profile에 연결한다.
    app.registerHandler(
        "/api/members/profile",
        [this](const drogon::HttpRequestPtr &request, Callback &&callback) {
            handleUpdateProfile(request, std::move(callback));
        },
        {drogon::Put});

    // Food MBTI 저장 엔드포인트를 PUT /api/members/me/food-mbti에 연결한다.
    app.registerHandler(
        "/api/members/me/food-mbti",
        [this](const drogon::HttpRequestPtr &request, Callback &&callback) {
            handleSaveFoodMbti(request, std::move(callback));
        },
        {drogon::Put});

    // 비밀번호 변경 엔드포인트를 PUT /api/members/password에 연결한다.
    app.registerHandler(
        "/api/members/password",
        [this](const drogon::HttpRequestPtr &request, Callback &&callback) {
            handleChangePassword(request, std::move(callback));
        },
        {drogon::Put});

    // 프론트 활동 이벤트를 받아 서버에서 경험치/레벨을 계산해 반영한다.
    app.registerHandler(
        "/api/members/exp",
        [this](const drogon::HttpRequestPtr &request, Callback &&callback) {
            handleAwardExperience(request, std::move(callback));
        },
        {drogon::Post});

    // 서버 메모리의 세션을 제거하고 브라우저 쿠키도 만료시킨다.
    app.registerHandler(
        "/api/auth/logout",
        [this](const drogon::HttpRequestPtr &request, Callback &&callback) {
            handleLogout(request, std::move(callback));
        },
        {drogon::Post});
}

Json::Value MemberController::buildMemberJson(const MemberDTO &member)
{
    Json::Value memberJson;
    // memberId는 64비트 정수라 JsonCpp 전용 타입으로 변환해서 넣는다.
    memberJson["memberId"] = static_cast<Json::UInt64>(member.memberId);
    memberJson["email"] = member.email;
    memberJson["name"] = member.name;
    memberJson["role"] = member.role;
    memberJson["status"] = member.status;
    memberJson["level"] = member.level;
    memberJson["exp"] = member.exp;
    memberJson["loggedIn"] = true;
    // lastLoginAt은 최초 가입 직전에는 없을 수 있으므로 값이 있을 때만 응답에 포함한다.
    if (member.lastLoginAt.has_value())
    {
        memberJson["lastLoginAt"] = *member.lastLoginAt;
    }
    return memberJson;
}

Json::Value MemberController::buildFoodMbtiJson(const FoodMbtiDTO &foodMbti)
{
    // Food MBTI 정보를 담을 JSON 객체를 생성한다.
    Json::Value item;
    // 단일 문자열 필드를 그대로 JSON에 매핑한다.
    item["type"] = foodMbti.type;
    item["title"] = foodMbti.title;
    item["description"] = foodMbti.description;

    // traits 벡터를 JSON 배열로 변환한다.
    Json::Value traits(Json::arrayValue);
    for (const auto &trait : foodMbti.traits)
    {
        // 각 특성 문자열을 배열의 끝에 추가한다.
        traits.append(trait);
    }
    item["traits"] = traits;

    // 추천 음식 목록도 동일하게 JSON 배열로 변환한다.
    Json::Value recommendedFoods(Json::arrayValue);
    for (const auto &food : foodMbti.recommendedFoods)
    {
        // 각 음식 문자열을 배열에 추가한다.
        recommendedFoods.append(food);
    }
    item["recommendedFoods"] = recommendedFoods;

    // 검사 완료 시각을 응답에 포함한다.
    item["completedAt"] = foodMbti.completedAt;
    // 직렬화된 결과 객체를 반환한다.
    return item;
}

Json::Value MemberController::buildProfileJson(const MemberProfileDTO &profile)
{
    // 프로필 응답 JSON 객체를 생성한다.
    Json::Value item;
    // 기본 프로필 필드를 DTO에서 JSON으로 복사한다.
    item["memberId"] = static_cast<Json::UInt64>(profile.memberId);
    item["name"] = profile.name;
    item["email"] = profile.email;
    item["level"] = profile.level;
    item["exp"] = profile.exp;
    item["isMe"] = profile.isMe;
    // Food MBTI 정보는 선택 값이므로 존재할 때만 객체로 채운다.
    if (profile.foodMbti.has_value())
    {
        item["foodMbti"] = buildFoodMbtiJson(*profile.foodMbti);
    }
    else
    {
        item["foodMbti"] = Json::nullValue;
    }
    return item;
}

void MemberController::applyCors(const drogon::HttpRequestPtr &request,
                                 const drogon::HttpResponsePtr &response)
{
    // 개발 환경에서 프론트엔드 origin이 달라도 쿠키 기반 인증 요청을 허용한다.
    std::string origin = request->getHeader("Origin");
    if (origin.empty())
    {
        origin = request->getHeader("origin");
    }

    if (!origin.empty())
    {
        response->addHeader("Access-Control-Allow-Origin", origin);
        response->addHeader("Vary", "Origin");
    }

    response->addHeader("Access-Control-Allow-Credentials", "true");
    response->addHeader("Access-Control-Allow-Headers", "Content-Type");
    response->addHeader("Access-Control-Allow-Methods", "GET, POST, PUT, OPTIONS");
}

std::string MemberController::extractSessionToken(
    const drogon::HttpRequestPtr &request)
{
    // Drogon이 파싱한 Cookie 헤더에서 isbox_session 값만 꺼낸다.
    return request->getCookie(kSessionCookieName);
}

std::optional<std::uint64_t> MemberController::parseMemberId(
    const std::string &memberIdValue)
{
    // 경로에서 받은 memberId 문자열이 비어 있으면 유효하지 않은 값이다.
    if (memberIdValue.empty())
    {
        return std::nullopt;
    }

    try
    {
        // 숫자 문자열을 부호 없는 64비트 정수로 변환한다.
        const auto parsed = std::stoull(memberIdValue);
        // 서비스에서 0은 유효한 회원 ID가 아니므로 실패로 처리한다.
        if (parsed == 0)
        {
            return std::nullopt;
        }
        // 정상 파싱된 값을 반환한다.
        return parsed;
    }
    // 숫자가 아니거나 범위를 벗어나면 파싱 실패로 처리한다.
    catch (const std::exception &)
    {
        return std::nullopt;
    }
}

std::string MemberController::buildSessionCookie(const std::string &token,
                                                 int maxAgeSeconds)
{
    // 쿠키 헤더 문자열을 단계적으로 조립하기 위해 스트림을 사용한다.
    std::ostringstream cookie;
    // HttpOnly로 JS 접근을 막고, SameSite=Lax로 일반 페이지 이동 흐름의 쿠키 전송은 유지한다.
    cookie << kSessionCookieName << '=' << token
           << "; Path=/; HttpOnly; SameSite=Lax; Max-Age=" << maxAgeSeconds;
    return cookie.str();
}

void MemberController::handleSignup(const drogon::HttpRequestPtr &request,
                                    Callback &&callback)
{
    // 회원가입 API는 JSON body만 허용한다.
    const auto json = request->getJsonObject();
    if (!json)
    {
        auto response =
            drogon::HttpResponse::newHttpJsonResponse(makeErrorBody("Invalid JSON body."));
        response->setStatusCode(drogon::k400BadRequest);
        applyCors(request, response);
        callback(response);
        return;
    }

    // HTTP JSON 필드를 서비스 계층이 이해하는 회원가입 DTO로 옮긴다.
    SignupRequestDTO dto;
    dto.name = json->get("name", "").asString();
    dto.email = json->get("email", "").asString();
    dto.password = json->get("password", "").asString();
    dto.confirmPassword = json->get("confirmPassword", "").asString();
    dto.agreeTerms = json->get("agreeTerms", false).asBool();

    const auto result = service_.signup(dto);

    // 서비스 결과를 공통 응답 형태로 변환한다.
    Json::Value body;
    body["ok"] = result.ok;
    body["message"] = result.message;
    if (result.member.has_value())
    {
        body["member"] = buildMemberJson(*result.member);
    }

    auto response = drogon::HttpResponse::newHttpJsonResponse(body);
    response->setStatusCode(static_cast<drogon::HttpStatusCode>(result.statusCode));
    if (result.ok && result.sessionToken.has_value())
    {
        // 회원가입 성공 시 자동 로그인되도록 세션 쿠키를 내려준다.
        response->addHeader("Set-Cookie", buildSessionCookie(*result.sessionToken, 86400));
    }

    applyCors(request, response);
    callback(response);
}

void MemberController::handleLogin(const drogon::HttpRequestPtr &request,
                                   Callback &&callback)
{
    // 로그인도 JSON body를 기준으로만 처리한다.
    const auto json = request->getJsonObject();
    if (!json)
    {
        auto response =
            drogon::HttpResponse::newHttpJsonResponse(makeErrorBody("Invalid JSON body."));
        response->setStatusCode(drogon::k400BadRequest);
        applyCors(request, response);
        callback(response);
        return;
    }

    // 이메일과 비밀번호만 서비스로 넘긴다. 검증과 비밀번호 비교는 서비스가 담당한다.
    LoginRequestDTO dto;
    dto.email = json->get("email", "").asString();
    dto.password = json->get("password", "").asString();

    const auto result = service_.login(dto);

    Json::Value body;
    body["ok"] = result.ok;
    body["message"] = result.message;
    if (result.member.has_value())
    {
        body["member"] = buildMemberJson(*result.member);
    }

    auto response = drogon::HttpResponse::newHttpJsonResponse(body);
    response->setStatusCode(static_cast<drogon::HttpStatusCode>(result.statusCode));
    if (result.ok && result.sessionToken.has_value())
    {
        // 로그인 성공 시 이후 /api/members/me 요청에서 사용할 세션 쿠키를 저장시킨다.
        response->addHeader("Set-Cookie", buildSessionCookie(*result.sessionToken, 86400));
    }

    applyCors(request, response);
    callback(response);
}

void MemberController::handleMe(const drogon::HttpRequestPtr &request,
                                Callback &&callback)
{
    // 현재 사용자 조회는 body 없이 쿠키에 들어 있는 세션 토큰만 확인한다.
    const auto sessionToken = extractSessionToken(request);
    const auto member = service_.getCurrentMember(sessionToken);

    if (!member.has_value())
    {
        auto response = drogon::HttpResponse::newHttpJsonResponse(makeErrorBody("Unauthorized."));
        response->setStatusCode(drogon::k401Unauthorized);
        applyCors(request, response);
        callback(response);
        return;
    }

    Json::Value body;
    body["ok"] = true;
    body["member"] = buildMemberJson(*member);

    auto response = drogon::HttpResponse::newHttpJsonResponse(body);
    response->setStatusCode(drogon::k200OK);
    applyCors(request, response);
    callback(response);
}

void MemberController::handleGetMyProfile(const drogon::HttpRequestPtr &request,
                                          Callback &&callback)
{
    // 쿠키의 세션 토큰으로 현재 로그인 사용자의 프로필을 조회한다.
    const auto result = service_.getMyProfile(extractSessionToken(request));

    // 서비스 결과를 공통 JSON 응답 형식으로 조립한다.
    Json::Value body;
    body["ok"] = result.ok;
    body["message"] = result.message;
    if (result.profile.has_value())
    {
        // 프로필이 있으면 직렬화하여 profile 필드에 포함한다.
        body["profile"] = buildProfileJson(*result.profile);
    }

    // 서비스 상태코드를 HTTP 응답 코드로 반영한다.
    auto response = drogon::HttpResponse::newHttpJsonResponse(body);
    response->setStatusCode(static_cast<drogon::HttpStatusCode>(result.statusCode));
    // 브라우저에서 쿠키 인증 호출이 가능하도록 CORS 헤더를 적용한다.
    applyCors(request, response);
    // 완성된 응답을 콜백으로 반환한다.
    callback(response);
}

void MemberController::handleGetMemberProfile(
    const drogon::HttpRequestPtr &request,
    Callback &&callback,
    const std::string &memberIdValue)
{
    // URL 경로 변수 문자열을 안전하게 회원 ID 숫자로 변환한다.
    const auto memberId = parseMemberId(memberIdValue);
    if (!memberId.has_value())
    {
        // ID 형식이 잘못된 경우 400 Bad Request로 즉시 응답한다.
        auto response =
            drogon::HttpResponse::newHttpJsonResponse(makeErrorBody("Invalid member id."));
        response->setStatusCode(drogon::k400BadRequest);
        applyCors(request, response);
        callback(response);
        return;
    }

    // 세션 토큰과 대상 회원 ID를 전달해 프로필 조회를 서비스에 위임한다.
    const auto result =
        service_.getMemberProfile(extractSessionToken(request), *memberId);

    // 서비스 결과를 공통 JSON 응답 형태로 변환한다.
    Json::Value body;
    body["ok"] = result.ok;
    body["message"] = result.message;
    if (result.profile.has_value())
    {
        // 조회 성공 시 profile 객체를 응답 본문에 추가한다.
        body["profile"] = buildProfileJson(*result.profile);
    }

    // 상태코드, CORS를 설정한 뒤 응답을 반환한다.
    auto response = drogon::HttpResponse::newHttpJsonResponse(body);
    response->setStatusCode(static_cast<drogon::HttpStatusCode>(result.statusCode));
    applyCors(request, response);
    callback(response);
}

void MemberController::handleUpdateProfile(const drogon::HttpRequestPtr &request,
                                           Callback &&callback)
{
    // 프로필 수정 API는 JSON body가 필수이다.
    const auto json = request->getJsonObject();
    if (!json)
    {
        auto response =
            drogon::HttpResponse::newHttpJsonResponse(makeErrorBody("Invalid JSON body."));
        response->setStatusCode(drogon::k400BadRequest);
        applyCors(request, response);
        callback(response);
        return;
    }

    // 수정 가능한 입력 필드만 DTO로 추출한다.
    UpdateProfileRequestDTO dto;
    dto.name = json->get("name", "").asString();
    dto.email = json->get("email", "").asString();

    // 세션 토큰으로 현재 사용자를 식별해 서비스 로직을 호출한다.
    const auto sessionToken = extractSessionToken(request);
    const auto result = service_.updateProfile(sessionToken, dto);

    // 서비스 결과를 JSON 응답으로 구성한다.
    Json::Value body;
    body["ok"] = result.ok;
    body["message"] = result.message;
    if (result.member.has_value())
    {
        // 변경된 회원 정보가 있으면 member 객체를 포함한다.
        body["member"] = buildMemberJson(*result.member);
    }

    // 상태코드와 CORS 헤더를 설정하고 응답한다.
    auto response = drogon::HttpResponse::newHttpJsonResponse(body);
    response->setStatusCode(static_cast<drogon::HttpStatusCode>(result.statusCode));
    applyCors(request, response);
    callback(response);
}

void MemberController::handleChangePassword(const drogon::HttpRequestPtr &request,
                                            Callback &&callback)
{
    // 비밀번호 변경 API는 JSON body를 필수로 요구한다.
    const auto json = request->getJsonObject();
    if (!json)
    {
        auto response =
            drogon::HttpResponse::newHttpJsonResponse(makeErrorBody("Invalid JSON body."));
        response->setStatusCode(drogon::k400BadRequest);
        applyCors(request, response);
        callback(response);
        return;
    }

    // 현재/신규/확인 비밀번호를 DTO로 매핑한다.
    ChangePasswordRequestDTO dto;
    dto.currentPassword = json->get("currentPassword", "").asString();
    dto.newPassword = json->get("newPassword", "").asString();
    dto.confirmPassword = json->get("confirmPassword", "").asString();

    // 세션 토큰으로 사용자 식별 후 비밀번호 변경 로직을 실행한다.
    const auto sessionToken = extractSessionToken(request);
    const auto result = service_.changePassword(sessionToken, dto);

    // 성공/실패 결과와 메시지를 응답 본문으로 조립한다.
    Json::Value body;
    body["ok"] = result.ok;
    body["message"] = result.message;

    // 상태코드/CORS를 적용하고 응답을 반환한다.
    auto response = drogon::HttpResponse::newHttpJsonResponse(body);
    response->setStatusCode(static_cast<drogon::HttpStatusCode>(result.statusCode));
    applyCors(request, response);
    callback(response);
}

void MemberController::handleAwardExperience(const drogon::HttpRequestPtr &request,
                                             Callback &&callback)
{
    // 경험치 지급 API는 JSON body를 필수로 요구한다.
    const auto json = request->getJsonObject();
    if (!json)
    {
        auto response =
            drogon::HttpResponse::newHttpJsonResponse(makeErrorBody("Invalid JSON body."));
        response->setStatusCode(drogon::k400BadRequest);
        applyCors(request, response);
        callback(response);
        return;
    }

    // 어떤 행동으로 경험치를 부여할지 actionType을 DTO에 담는다.
    AwardExperienceRequestDTO dto;
    dto.actionType = json->get("actionType", "").asString();

    // 세션 토큰으로 사용자를 식별해 경험치 반영 로직을 실행한다.
    const auto sessionToken = extractSessionToken(request);
    const auto result = service_.awardExperience(sessionToken, dto);

    // 지급 결과와 레벨 변화를 포함한 응답 본문을 조립한다.
    Json::Value body;
    body["ok"] = result.ok;
    body["message"] = result.message;
    body["awardedExp"] = result.awardedExp;
    body["previousLevel"] = result.previousLevel;
    body["newLevel"] = result.newLevel;
    if (result.member.has_value())
    {
        // 최신 회원 상태를 내려주기 위해 member 정보를 함께 포함한다.
        body["member"] = buildMemberJson(*result.member);
    }

    // 상태코드/CORS를 적용해 응답을 반환한다.
    auto response = drogon::HttpResponse::newHttpJsonResponse(body);
    response->setStatusCode(static_cast<drogon::HttpStatusCode>(result.statusCode));
    applyCors(request, response);
    callback(response);
}

void MemberController::handleSaveFoodMbti(const drogon::HttpRequestPtr &request,
                                          Callback &&callback)
{
    // Food MBTI 저장 API는 JSON body를 필수로 요구한다.
    const auto json = request->getJsonObject();
    if (!json)
    {
        auto response =
            drogon::HttpResponse::newHttpJsonResponse(makeErrorBody("Invalid JSON body."));
        response->setStatusCode(drogon::k400BadRequest);
        applyCors(request, response);
        callback(response);
        return;
    }

    // 기본 문자열 필드를 요청 JSON에서 DTO로 복사한다.
    SaveFoodMbtiRequestDTO dto;
    dto.type = json->get("type", "").asString();
    dto.title = json->get("title", "").asString();
    dto.description = json->get("description", "").asString();

    // traits가 배열로 들어오면 문자열 원소만 선별해 DTO에 담는다.
    if (json->isMember("traits") && (*json)["traits"].isArray())
    {
        for (const auto &item : (*json)["traits"])
        {
            if (item.isString())
            {
                // 문자열 값만 허용해 타입 오염을 방지한다.
                dto.traits.push_back(item.asString());
            }
        }
    }

    // recommendedFoods도 같은 방식으로 문자열 배열만 복사한다.
    if (json->isMember("recommendedFoods") &&
        (*json)["recommendedFoods"].isArray())
    {
        for (const auto &item : (*json)["recommendedFoods"])
        {
            if (item.isString())
            {
                // 문자열 값만 허용해 타입 오염을 방지한다.
                dto.recommendedFoods.push_back(item.asString());
            }
        }
    }

    // completedAt은 선택 필드이므로 문자열일 때만 반영한다.
    if (json->isMember("completedAt") && (*json)["completedAt"].isString())
    {
        dto.completedAt = (*json)["completedAt"].asString();
    }

    // 세션 토큰 기준으로 현재 사용자 MBTI 저장을 서비스에 위임한다.
    const auto result = service_.saveMyFoodMbti(extractSessionToken(request), dto);

    // 서비스 결과를 JSON 응답으로 조립한다.
    Json::Value body;
    body["ok"] = result.ok;
    body["message"] = result.message;
    if (result.foodMbti.has_value())
    {
        // 저장된 MBTI 결과가 있을 때만 응답에 포함한다.
        body["foodMbti"] = buildFoodMbtiJson(*result.foodMbti);
    }

    // 상태코드/CORS를 적용하고 응답을 반환한다.
    auto response = drogon::HttpResponse::newHttpJsonResponse(body);
    response->setStatusCode(static_cast<drogon::HttpStatusCode>(result.statusCode));
    applyCors(request, response);
    callback(response);
}

void MemberController::handleLogout(const drogon::HttpRequestPtr &request,
                                    Callback &&callback)
{
    // 서버 세션 저장소에서 토큰을 제거한다. 빈 토큰이어도 성공 응답을 내려 멱등성을 유지한다.
    const auto sessionToken = extractSessionToken(request);
    service_.logout(sessionToken);

    Json::Value body;
    body["ok"] = true;
    body["message"] = "Logout success.";

    auto response = drogon::HttpResponse::newHttpJsonResponse(body);
    response->setStatusCode(drogon::k200OK);
    // Max-Age=0 쿠키를 내려 브라우저가 저장한 세션 쿠키를 지우게 한다.
    response->addHeader("Set-Cookie", buildSessionCookie("", 0));
    applyCors(request, response);
    callback(response);
}

}  // namespace auth
