/*
 * 파일 개요: 회원 인증/프로필 HTTP 요청을 실제로 처리하는 컨트롤러 구현 파일이다.
 * 주요 역할: Drogon 라우팅 등록, 요청 JSON 파싱, 서비스 호출, 공통 응답 포맷 구성.
 */
#include "member/controller/MemberController.h"

// Set-Cookie 문자열 조합에 사용한다.
#include <sstream>
// 콜백 전달 시 move를 사용한다.
#include <utility>

namespace
{

// 인증 세션 쿠키 이름(클라이언트/서버 공통 식별자).
constexpr const char *kSessionCookieName = "isbox_session";

// 컨트롤러에서 공통으로 사용하는 에러 응답 본문 형태다.
Json::Value makeErrorBody(const std::string &message)
{
    Json::Value body;
    body["ok"] = false;
    body["message"] = message;
    return body;
}

}  // namespace

namespace auth
{

void MemberController::registerHandlers()
{
    // 라우팅 등록은 앱 전역 인스턴스에 수행한다.
    auto &app = drogon::app();

    // 인증 API
    app.registerHandler(
        "/api/auth/signup",
        [this](const drogon::HttpRequestPtr &request, Callback &&callback) {
            handleSignup(request, std::move(callback));
        },
        {drogon::Post});

    app.registerHandler(
        "/api/auth/login",
        [this](const drogon::HttpRequestPtr &request, Callback &&callback) {
            handleLogin(request, std::move(callback));
        },
        {drogon::Post});

    app.registerHandler(
        "/api/auth/logout",
        [this](const drogon::HttpRequestPtr &request, Callback &&callback) {
            handleLogout(request, std::move(callback));
        },
        {drogon::Post});

    // 회원 정보 API
    app.registerHandler(
        "/api/members/me",
        [this](const drogon::HttpRequestPtr &request, Callback &&callback) {
            handleMe(request, std::move(callback));
        },
        {drogon::Get});

    app.registerHandler(
        "/api/members/profile",
        [this](const drogon::HttpRequestPtr &request, Callback &&callback) {
            handleGetMyProfile(request, std::move(callback));
        },
        {drogon::Get});

    app.registerHandler(
        "/api/members/{1}/profile",
        [this](const drogon::HttpRequestPtr &request,
               Callback &&callback,
               const std::string &memberIdValue) {
            handleGetMemberProfile(request, std::move(callback), memberIdValue);
        },
        {drogon::Get});

    app.registerHandler(
        "/api/members/profile",
        [this](const drogon::HttpRequestPtr &request, Callback &&callback) {
            handleUpdateProfile(request, std::move(callback));
        },
        {drogon::Put});

    app.registerHandler(
        "/api/members/password",
        [this](const drogon::HttpRequestPtr &request, Callback &&callback) {
            handleChangePassword(request, std::move(callback));
        },
        {drogon::Put});

    app.registerHandler(
        "/api/members/exp",
        [this](const drogon::HttpRequestPtr &request, Callback &&callback) {
            handleAwardExperience(request, std::move(callback));
        },
        {drogon::Post});

    app.registerHandler(
        "/api/members/me/food-mbti",
        [this](const drogon::HttpRequestPtr &request, Callback &&callback) {
            handleSaveFoodMbti(request, std::move(callback));
        },
        {drogon::Put});
}

Json::Value MemberController::buildMemberJson(const MemberDTO &member)
{
    Json::Value memberJson;
    memberJson["memberId"] = static_cast<Json::UInt64>(member.memberId);
    memberJson["email"] = member.email;
    memberJson["name"] = member.name;
    memberJson["role"] = member.role;
    memberJson["status"] = member.status;
    memberJson["level"] = member.level;
    memberJson["exp"] = member.exp;
    memberJson["loggedIn"] = true;
    if (member.lastLoginAt.has_value())
    {
        memberJson["lastLoginAt"] = *member.lastLoginAt;
    }
    return memberJson;
}

Json::Value MemberController::buildFoodMbtiJson(const FoodMbtiDTO &foodMbti)
{
    Json::Value item;
    item["type"] = foodMbti.type;
    item["title"] = foodMbti.title;
    item["description"] = foodMbti.description;

    Json::Value traits(Json::arrayValue);
    for (const auto &trait : foodMbti.traits)
    {
        traits.append(trait);
    }
    item["traits"] = traits;

    Json::Value recommendedFoods(Json::arrayValue);
    for (const auto &food : foodMbti.recommendedFoods)
    {
        recommendedFoods.append(food);
    }
    item["recommendedFoods"] = recommendedFoods;
    item["completedAt"] = foodMbti.completedAt;
    return item;
}

Json::Value MemberController::buildProfileJson(const MemberProfileDTO &profile)
{
    Json::Value item;
    item["memberId"] = static_cast<Json::UInt64>(profile.memberId);
    item["name"] = profile.name;
    item["email"] = profile.email;
    item["level"] = profile.level;
    item["exp"] = profile.exp;
    item["isMe"] = profile.isMe;
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
    // 요청 Origin을 그대로 반영해 쿠키 인증 CORS를 허용한다.
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
    return request->getCookie(kSessionCookieName);
}

std::optional<std::uint64_t> MemberController::parseMemberId(
    const std::string &memberIdValue)
{
    if (memberIdValue.empty())
    {
        return std::nullopt;
    }

    try
    {
        const auto parsed = std::stoull(memberIdValue);
        if (parsed == 0)
        {
            return std::nullopt;
        }
        return parsed;
    }
    catch (const std::exception &)
    {
        return std::nullopt;
    }
}

std::string MemberController::buildSessionCookie(const std::string &token,
                                                 int maxAgeSeconds)
{
    std::ostringstream cookie;
    cookie << kSessionCookieName << '=' << token
           << "; Path=/; HttpOnly; SameSite=Lax; Max-Age=" << maxAgeSeconds;
    return cookie.str();
}

void MemberController::handleSignup(const drogon::HttpRequestPtr &request,
                                    Callback &&callback)
{
    // 공통 흐름: 입력(JSON) 검증 -> DTO 변환 -> 서비스 호출 -> JSON 응답 생성.
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

    SignupRequestDTO dto;
    dto.name = json->get("name", "").asString();
    dto.email = json->get("email", "").asString();
    dto.password = json->get("password", "").asString();
    dto.confirmPassword = json->get("confirmPassword", "").asString();
    dto.agreeTerms = json->get("agreeTerms", false).asBool();

    const auto result = service_.signup(dto);

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
        // 가입 성공 시 즉시 로그인 상태를 만들기 위해 세션 쿠키를 발급한다.
        response->addHeader("Set-Cookie", buildSessionCookie(*result.sessionToken, 86400));
    }

    applyCors(request, response);
    callback(response);
}

void MemberController::handleLogin(const drogon::HttpRequestPtr &request,
                                   Callback &&callback)
{
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
        response->addHeader("Set-Cookie", buildSessionCookie(*result.sessionToken, 86400));
    }

    applyCors(request, response);
    callback(response);
}

void MemberController::handleMe(const drogon::HttpRequestPtr &request,
                                Callback &&callback)
{
    // body 없이 쿠키의 세션 토큰으로 현재 사용자만 조회한다.
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
    const auto result = service_.getMyProfile(extractSessionToken(request));

    Json::Value body;
    body["ok"] = result.ok;
    body["message"] = result.message;
    if (result.profile.has_value())
    {
        body["profile"] = buildProfileJson(*result.profile);
    }

    auto response = drogon::HttpResponse::newHttpJsonResponse(body);
    response->setStatusCode(static_cast<drogon::HttpStatusCode>(result.statusCode));
    applyCors(request, response);
    callback(response);
}

void MemberController::handleGetMemberProfile(
    const drogon::HttpRequestPtr &request,
    Callback &&callback,
    const std::string &memberIdValue)
{
    // 경로 파라미터는 먼저 형식 검증을 통과해야 한다.
    const auto memberId = parseMemberId(memberIdValue);
    if (!memberId.has_value())
    {
        auto response =
            drogon::HttpResponse::newHttpJsonResponse(makeErrorBody("Invalid member id."));
        response->setStatusCode(drogon::k400BadRequest);
        applyCors(request, response);
        callback(response);
        return;
    }

    const auto result =
        service_.getMemberProfile(extractSessionToken(request), *memberId);

    Json::Value body;
    body["ok"] = result.ok;
    body["message"] = result.message;
    if (result.profile.has_value())
    {
        body["profile"] = buildProfileJson(*result.profile);
    }

    auto response = drogon::HttpResponse::newHttpJsonResponse(body);
    response->setStatusCode(static_cast<drogon::HttpStatusCode>(result.statusCode));
    applyCors(request, response);
    callback(response);
}

void MemberController::handleUpdateProfile(const drogon::HttpRequestPtr &request,
                                           Callback &&callback)
{
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

    UpdateProfileRequestDTO dto;
    dto.name = json->get("name", "").asString();
    dto.email = json->get("email", "").asString();

    const auto sessionToken = extractSessionToken(request);
    const auto result = service_.updateProfile(sessionToken, dto);

    Json::Value body;
    body["ok"] = result.ok;
    body["message"] = result.message;
    if (result.member.has_value())
    {
        body["member"] = buildMemberJson(*result.member);
    }

    auto response = drogon::HttpResponse::newHttpJsonResponse(body);
    response->setStatusCode(static_cast<drogon::HttpStatusCode>(result.statusCode));
    applyCors(request, response);
    callback(response);
}

void MemberController::handleChangePassword(const drogon::HttpRequestPtr &request,
                                            Callback &&callback)
{
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

    ChangePasswordRequestDTO dto;
    dto.currentPassword = json->get("currentPassword", "").asString();
    dto.newPassword = json->get("newPassword", "").asString();
    dto.confirmPassword = json->get("confirmPassword", "").asString();

    const auto sessionToken = extractSessionToken(request);
    const auto result = service_.changePassword(sessionToken, dto);

    Json::Value body;
    body["ok"] = result.ok;
    body["message"] = result.message;

    auto response = drogon::HttpResponse::newHttpJsonResponse(body);
    response->setStatusCode(static_cast<drogon::HttpStatusCode>(result.statusCode));
    applyCors(request, response);
    callback(response);
}

void MemberController::handleAwardExperience(const drogon::HttpRequestPtr &request,
                                             Callback &&callback)
{
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

    AwardExperienceRequestDTO dto;
    dto.actionType = json->get("actionType", "").asString();

    const auto sessionToken = extractSessionToken(request);
    const auto result = service_.awardExperience(sessionToken, dto);

    Json::Value body;
    body["ok"] = result.ok;
    body["message"] = result.message;
    body["awardedExp"] = result.awardedExp;
    body["previousLevel"] = result.previousLevel;
    body["newLevel"] = result.newLevel;
    if (result.member.has_value())
    {
        body["member"] = buildMemberJson(*result.member);
    }

    auto response = drogon::HttpResponse::newHttpJsonResponse(body);
    response->setStatusCode(static_cast<drogon::HttpStatusCode>(result.statusCode));
    applyCors(request, response);
    callback(response);
}

void MemberController::handleSaveFoodMbti(const drogon::HttpRequestPtr &request,
                                          Callback &&callback)
{
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

    SaveFoodMbtiRequestDTO dto;
    dto.type = json->get("type", "").asString();
    dto.title = json->get("title", "").asString();
    dto.description = json->get("description", "").asString();

    // 배열 입력은 문자열 원소만 추출해 DTO에 담는다.
    if (json->isMember("traits") && (*json)["traits"].isArray())
    {
        for (const auto &item : (*json)["traits"])
        {
            if (item.isString())
            {
                dto.traits.push_back(item.asString());
            }
        }
    }

    if (json->isMember("recommendedFoods") &&
        (*json)["recommendedFoods"].isArray())
    {
        for (const auto &item : (*json)["recommendedFoods"])
        {
            if (item.isString())
            {
                dto.recommendedFoods.push_back(item.asString());
            }
        }
    }

    if (json->isMember("completedAt") && (*json)["completedAt"].isString())
    {
        dto.completedAt = (*json)["completedAt"].asString();
    }

    const auto result = service_.saveMyFoodMbti(extractSessionToken(request), dto);

    Json::Value body;
    body["ok"] = result.ok;
    body["message"] = result.message;
    if (result.foodMbti.has_value())
    {
        body["foodMbti"] = buildFoodMbtiJson(*result.foodMbti);
    }

    auto response = drogon::HttpResponse::newHttpJsonResponse(body);
    response->setStatusCode(static_cast<drogon::HttpStatusCode>(result.statusCode));
    applyCors(request, response);
    callback(response);
}

void MemberController::handleLogout(const drogon::HttpRequestPtr &request,
                                    Callback &&callback)
{
    // 서버 세션 제거는 멱등적으로 처리한다(없는 토큰이어도 성공 응답).
    const auto sessionToken = extractSessionToken(request);
    service_.logout(sessionToken);

    Json::Value body;
    body["ok"] = true;
    body["message"] = "Logout success.";

    auto response = drogon::HttpResponse::newHttpJsonResponse(body);
    response->setStatusCode(drogon::k200OK);
    // 브라우저 쿠키 만료로 클라이언트 세션도 정리한다.
    response->addHeader("Set-Cookie", buildSessionCookie("", 0));
    applyCors(request, response);
    callback(response);
}

}  // namespace auth

