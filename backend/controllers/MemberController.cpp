#include "MemberController.h"

#include <sstream>
#include <utility>

namespace
{

// 브라우저에 저장되는 로그인 세션 쿠키 이름이다.
constexpr const char *kSessionCookieName = "isbox_session";

// Controller 단계에서 바로 반환해야 하는 오류 응답의 공통 JSON 형태를 만든다.
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

std::string MemberController::buildSessionCookie(const std::string &token,
                                                 int maxAgeSeconds)
{
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
