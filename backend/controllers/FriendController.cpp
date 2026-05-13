#include "FriendController.h"

// std::move 사용
#include <utility>

namespace
{

// 세션 식별 쿠키 키 이름(서버/클라이언트 간 약속)
constexpr const char *kSessionCookieName = "isbox_session";

Json::Value makeErrorBody(const std::string &message)
{
    // 공통 에러 응답 JSON 골격 생성
    Json::Value body;
    // 처리 성공 여부
    body["ok"] = false;
    // 에러 메시지
    body["message"] = message;
    return body;
}

}  // namespace

namespace friendship
{

void FriendController::registerHandlers()
{
    // 전역 Drogon 애플리케이션 객체 참조
    auto &app = drogon::app();

    // GET /api/friends/search : 이메일 기반 회원 검색
    app.registerHandler(
        "/api/friends/search",
        [this](const drogon::HttpRequestPtr &request, Callback &&callback) {
            // 실제 처리 로직은 전용 핸들러 함수로 위임
            handleSearchMember(request, std::move(callback));
        },
        {drogon::Get});

    // POST /api/friends/requests : 친구 요청 전송
    app.registerHandler(
        "/api/friends/requests",
        [this](const drogon::HttpRequestPtr &request, Callback &&callback) {
            handleSendRequest(request, std::move(callback));
        },
        {drogon::Post});

    // GET /api/friends : 수락된 친구 목록 조회
    app.registerHandler(
        "/api/friends",
        [this](const drogon::HttpRequestPtr &request, Callback &&callback) {
            handleListFriends(request, std::move(callback));
        },
        {drogon::Get});

    // GET /api/friends/requests/incoming : 받은 친구 요청 목록 조회
    app.registerHandler(
        "/api/friends/requests/incoming",
        [this](const drogon::HttpRequestPtr &request, Callback &&callback) {
            handleListIncomingRequests(request, std::move(callback));
        },
        {drogon::Get});

    // GET /api/friends/requests/outgoing : 보낸 친구 요청 목록 조회
    app.registerHandler(
        "/api/friends/requests/outgoing",
        [this](const drogon::HttpRequestPtr &request, Callback &&callback) {
            handleListOutgoingRequests(request, std::move(callback));
        },
        {drogon::Get});

    // POST /api/friends/requests/{id}/accept : 친구 요청 수락
    app.registerHandler(
        "/api/friends/requests/{1}/accept",
        [this](const drogon::HttpRequestPtr &request,
               Callback &&callback,
               const std::string &friendshipIdValue) {
            // URL 경로에서 추출된 friendshipIdValue를 함께 전달
            handleAcceptRequest(request, std::move(callback), friendshipIdValue);
        },
        {drogon::Post});

    // POST /api/friends/requests/{id}/reject : 친구 요청 거절
    app.registerHandler(
        "/api/friends/requests/{1}/reject",
        [this](const drogon::HttpRequestPtr &request,
               Callback &&callback,
               const std::string &friendshipIdValue) {
            handleRejectRequest(request, std::move(callback), friendshipIdValue);
        },
        {drogon::Post});

    // POST /api/friends/requests/{id}/cancel : 보낸 친구 요청 취소
    app.registerHandler(
        "/api/friends/requests/{1}/cancel",
        [this](const drogon::HttpRequestPtr &request,
               Callback &&callback,
               const std::string &friendshipIdValue) {
            handleCancelRequest(request, std::move(callback), friendshipIdValue);
        },
        {drogon::Post});
}

Json::Value FriendController::buildMemberJson(const FriendMemberDTO &member)
{
    // FriendMemberDTO -> JSON 직렬화
    Json::Value out;
    // JavaScript 정밀도 이슈를 피하기 위해 Json::UInt64로 명시 변환
    out["memberId"] = static_cast<Json::UInt64>(member.memberId);
    // 회원 이름
    out["name"] = member.name;
    // 회원 이메일
    out["email"] = member.email;
    // 친구 관계 상태(도메인 문자열)
    out["status"] = member.status;
    return out;
}

Json::Value FriendController::buildFriendJson(const FriendDTO &friendItem)
{
    // FriendDTO -> JSON 직렬화
    Json::Value out;
    // 친구 관계 식별자
    out["friendshipId"] = static_cast<Json::UInt64>(friendItem.friendshipId);
    // 상대 회원 식별자
    out["memberId"] = static_cast<Json::UInt64>(friendItem.memberId);
    // 상대 회원 이름
    out["name"] = friendItem.name;
    // 상대 회원 이메일
    out["email"] = friendItem.email;
    // 현재 관계 상태
    out["status"] = friendItem.status;
    // 요청 생성 시각
    out["requestedAt"] = friendItem.requestedAt;
    // 응답 시각은 값이 있을 때만 포함
    if (friendItem.respondedAt.has_value())
    {
        out["respondedAt"] = *friendItem.respondedAt;
    }
    return out;
}

Json::Value FriendController::buildRequestJson(const FriendRequestDTO &request)
{
    // FriendRequestDTO -> JSON 직렬화
    Json::Value out;
    // 친구 요청 식별자
    out["friendshipId"] = static_cast<Json::UInt64>(request.friendshipId);
    // 요청 상대 회원 식별자
    out["memberId"] = static_cast<Json::UInt64>(request.memberId);
    // 요청 상대 이름
    out["name"] = request.name;
    // 요청 상대 이메일
    out["email"] = request.email;
    // 요청 상태
    out["status"] = request.status;
    // 요청 시각
    out["requestedAt"] = request.requestedAt;
    // 응답 시각은 있을 때만 포함
    if (request.respondedAt.has_value())
    {
        out["respondedAt"] = *request.respondedAt;
    }
    return out;
}

void FriendController::applyCors(const drogon::HttpRequestPtr &request,
                                 const drogon::HttpResponsePtr &response)
{
    // 브라우저가 보낸 Origin 헤더를 우선 추출
    std::string origin = request->getHeader("Origin");
    // 일부 프록시/클라이언트는 소문자 키를 보낼 수 있어 보완
    if (origin.empty())
    {
        origin = request->getHeader("origin");
    }

    // Origin이 있으면 해당 Origin을 허용하고 캐시 분기용 Vary 설정
    if (!origin.empty())
    {
        response->addHeader("Access-Control-Allow-Origin", origin);
        response->addHeader("Vary", "Origin");
    }

    // 쿠키 기반 인증 허용
    response->addHeader("Access-Control-Allow-Credentials", "true");
    // 허용 헤더 목록
    response->addHeader("Access-Control-Allow-Headers", "Content-Type");
    // 허용 메서드 목록
    response->addHeader("Access-Control-Allow-Methods",
                        "GET, POST, PUT, OPTIONS");
}

std::string FriendController::extractSessionToken(
    const drogon::HttpRequestPtr &request)
{
    // 세션 쿠키값을 그대로 반환(없으면 빈 문자열)
    return request->getCookie(kSessionCookieName);
}

std::optional<std::uint64_t> FriendController::parseFriendshipId(
    const std::string &friendshipIdValue)
{
    // 빈 문자열은 유효한 ID가 아니므로 실패
    if (friendshipIdValue.empty())
    {
        return std::nullopt;
    }

    try
    {
        // 문자열을 부호 없는 정수로 변환
        const auto parsed = std::stoull(friendshipIdValue);
        // 0은 비즈니스 규칙상 유효하지 않은 ID로 간주
        if (parsed == 0)
        {
            return std::nullopt;
        }
        // 정상 파싱된 ID 반환
        return parsed;
    }
    // 숫자 변환 실패(형식 오류/범위 초과) 시 실패 반환
    catch (const std::exception &)
    {
        return std::nullopt;
    }
}

std::optional<std::uint64_t> FriendController::extractCurrentMemberId(
    const drogon::HttpRequestPtr &request)
{
    // 세션 토큰으로 현재 로그인 사용자 ID를 조회
    return memberService_.getCurrentMemberId(extractSessionToken(request));
}

void FriendController::handleSearchMember(const drogon::HttpRequestPtr &request,
                                          Callback &&callback)
{
    // 1) 인증 사용자 확인
    const auto memberId = extractCurrentMemberId(request);
    if (!memberId.has_value())
    {
        // 인증 실패 시 401 응답
        auto response =
            drogon::HttpResponse::newHttpJsonResponse(makeErrorBody("Unauthorized."));
        response->setStatusCode(drogon::k401Unauthorized);
        applyCors(request, response);
        callback(response);
        return;
    }

    // 2) 쿼리 파라미터(email)로 회원 검색 수행
    const auto result =
        friendService_.searchMemberByEmail(*memberId, request->getParameter("email"));

    // 3) 서비스 결과를 공통 응답 포맷 JSON으로 구성
    Json::Value body;
    body["ok"] = result.ok;
    body["message"] = result.message;
    // 검색 결과 회원이 있을 때만 member 필드 포함
    if (result.member.has_value())
    {
        body["member"] = buildMemberJson(*result.member);
    }

    // 4) 상태코드와 CORS를 설정해 응답 반환
    auto response = drogon::HttpResponse::newHttpJsonResponse(body);
    response->setStatusCode(static_cast<drogon::HttpStatusCode>(result.statusCode));
    applyCors(request, response);
    callback(response);
}

void FriendController::handleSendRequest(const drogon::HttpRequestPtr &request,
                                         Callback &&callback)
{
    // 1) 인증 사용자 확인
    const auto memberId = extractCurrentMemberId(request);
    if (!memberId.has_value())
    {
        auto response =
            drogon::HttpResponse::newHttpJsonResponse(makeErrorBody("Unauthorized."));
        response->setStatusCode(drogon::k401Unauthorized);
        applyCors(request, response);
        callback(response);
        return;
    }

    // 2) JSON 본문 유효성 검사
    const auto json = request->getJsonObject();
    if (!json)
    {
        // JSON이 아니거나 파싱 실패면 400 반환
        auto response =
            drogon::HttpResponse::newHttpJsonResponse(makeErrorBody("Invalid JSON body."));
        response->setStatusCode(drogon::k400BadRequest);
        applyCors(request, response);
        callback(response);
        return;
    }

    // 3) 본문 email 값을 사용해 친구 요청 생성
    const auto result =
        friendService_.sendFriendRequest(*memberId, json->get("email", "").asString());

    // 4) 서비스 결과를 JSON으로 구성
    Json::Value body;
    body["ok"] = result.ok;
    body["message"] = result.message;
    // 생성된 요청 정보가 있으면 포함
    if (result.request.has_value())
    {
        body["request"] = buildRequestJson(*result.request);
    }

    // 5) 상태코드/CORS 적용 후 반환
    auto response = drogon::HttpResponse::newHttpJsonResponse(body);
    response->setStatusCode(static_cast<drogon::HttpStatusCode>(result.statusCode));
    applyCors(request, response);
    callback(response);
}

void FriendController::handleListFriends(const drogon::HttpRequestPtr &request,
                                         Callback &&callback)
{
    // 1) 인증 사용자 확인
    const auto memberId = extractCurrentMemberId(request);
    if (!memberId.has_value())
    {
        auto response =
            drogon::HttpResponse::newHttpJsonResponse(makeErrorBody("Unauthorized."));
        response->setStatusCode(drogon::k401Unauthorized);
        applyCors(request, response);
        callback(response);
        return;
    }

    // 2) 수락 완료된 친구 목록 조회
    const auto result = friendService_.listAcceptedFriends(*memberId);

    // 3) 응답 메타 필드 구성
    Json::Value body;
    body["ok"] = result.ok;
    body["message"] = result.message;

    // 4) DTO 목록을 JSON 배열로 직렬화
    Json::Value friendsJson(Json::arrayValue);
    for (const auto &friendItem : result.friends)
    {
        friendsJson.append(buildFriendJson(friendItem));
    }
    body["friends"] = friendsJson;

    // 5) 상태코드/CORS 적용 후 반환
    auto response = drogon::HttpResponse::newHttpJsonResponse(body);
    response->setStatusCode(static_cast<drogon::HttpStatusCode>(result.statusCode));
    applyCors(request, response);
    callback(response);
}

void FriendController::handleListIncomingRequests(
    const drogon::HttpRequestPtr &request,
    Callback &&callback)
{
    // 1) 인증 사용자 확인
    const auto memberId = extractCurrentMemberId(request);
    if (!memberId.has_value())
    {
        auto response =
            drogon::HttpResponse::newHttpJsonResponse(makeErrorBody("Unauthorized."));
        response->setStatusCode(drogon::k401Unauthorized);
        applyCors(request, response);
        callback(response);
        return;
    }

    // 2) 받은 친구 요청 목록 조회
    const auto result = friendService_.listIncomingRequests(*memberId);

    // 3) 공통 응답 필드 구성
    Json::Value body;
    body["ok"] = result.ok;
    body["message"] = result.message;

    // 4) 요청 DTO 목록을 JSON 배열로 변환
    Json::Value requestsJson(Json::arrayValue);
    for (const auto &item : result.requests)
    {
        requestsJson.append(buildRequestJson(item));
    }
    body["requests"] = requestsJson;

    // 5) 상태코드/CORS 적용 후 반환
    auto response = drogon::HttpResponse::newHttpJsonResponse(body);
    response->setStatusCode(static_cast<drogon::HttpStatusCode>(result.statusCode));
    applyCors(request, response);
    callback(response);
}

void FriendController::handleListOutgoingRequests(
    const drogon::HttpRequestPtr &request,
    Callback &&callback)
{
    // 1) 인증 사용자 확인
    const auto memberId = extractCurrentMemberId(request);
    if (!memberId.has_value())
    {
        auto response =
            drogon::HttpResponse::newHttpJsonResponse(makeErrorBody("Unauthorized."));
        response->setStatusCode(drogon::k401Unauthorized);
        applyCors(request, response);
        callback(response);
        return;
    }

    // 2) 보낸 친구 요청 목록 조회
    const auto result = friendService_.listOutgoingRequests(*memberId);

    // 3) 공통 응답 필드 구성
    Json::Value body;
    body["ok"] = result.ok;
    body["message"] = result.message;

    // 4) 요청 DTO 목록을 JSON 배열로 변환
    Json::Value requestsJson(Json::arrayValue);
    for (const auto &item : result.requests)
    {
        requestsJson.append(buildRequestJson(item));
    }
    body["requests"] = requestsJson;

    // 5) 상태코드/CORS 적용 후 반환
    auto response = drogon::HttpResponse::newHttpJsonResponse(body);
    response->setStatusCode(static_cast<drogon::HttpStatusCode>(result.statusCode));
    applyCors(request, response);
    callback(response);
}

void FriendController::handleAcceptRequest(const drogon::HttpRequestPtr &request,
                                           Callback &&callback,
                                           const std::string &friendshipIdValue)
{
    // 1) 인증 사용자 확인
    const auto memberId = extractCurrentMemberId(request);
    if (!memberId.has_value())
    {
        auto response =
            drogon::HttpResponse::newHttpJsonResponse(makeErrorBody("Unauthorized."));
        response->setStatusCode(drogon::k401Unauthorized);
        applyCors(request, response);
        callback(response);
        return;
    }

    // 2) 경로 변수 friendshipId 검증
    const auto friendshipId = parseFriendshipId(friendshipIdValue);
    if (!friendshipId.has_value())
    {
        // 유효하지 않은 ID 형식이면 400 반환
        auto response =
            drogon::HttpResponse::newHttpJsonResponse(makeErrorBody("Invalid friendship id."));
        response->setStatusCode(drogon::k400BadRequest);
        applyCors(request, response);
        callback(response);
        return;
    }

    // 3) 친구 요청 수락 처리
    const auto result = friendService_.acceptRequest(*memberId, *friendshipId);

    // 4) 결과 JSON 구성
    Json::Value body;
    body["ok"] = result.ok;
    body["message"] = result.message;
    // 갱신된 요청 데이터가 있으면 포함
    if (result.request.has_value())
    {
        body["request"] = buildRequestJson(*result.request);
    }

    // 5) 상태코드/CORS 적용 후 반환
    auto response = drogon::HttpResponse::newHttpJsonResponse(body);
    response->setStatusCode(static_cast<drogon::HttpStatusCode>(result.statusCode));
    applyCors(request, response);
    callback(response);
}

void FriendController::handleRejectRequest(const drogon::HttpRequestPtr &request,
                                           Callback &&callback,
                                           const std::string &friendshipIdValue)
{
    // 1) 인증 사용자 확인
    const auto memberId = extractCurrentMemberId(request);
    if (!memberId.has_value())
    {
        auto response =
            drogon::HttpResponse::newHttpJsonResponse(makeErrorBody("Unauthorized."));
        response->setStatusCode(drogon::k401Unauthorized);
        applyCors(request, response);
        callback(response);
        return;
    }

    // 2) 경로 변수 friendshipId 검증
    const auto friendshipId = parseFriendshipId(friendshipIdValue);
    if (!friendshipId.has_value())
    {
        auto response =
            drogon::HttpResponse::newHttpJsonResponse(makeErrorBody("Invalid friendship id."));
        response->setStatusCode(drogon::k400BadRequest);
        applyCors(request, response);
        callback(response);
        return;
    }

    // 3) 친구 요청 거절 처리
    const auto result = friendService_.rejectRequest(*memberId, *friendshipId);

    // 4) 결과 JSON 구성
    Json::Value body;
    body["ok"] = result.ok;
    body["message"] = result.message;
    // 갱신된 요청 데이터가 있으면 포함
    if (result.request.has_value())
    {
        body["request"] = buildRequestJson(*result.request);
    }

    // 5) 상태코드/CORS 적용 후 반환
    auto response = drogon::HttpResponse::newHttpJsonResponse(body);
    response->setStatusCode(static_cast<drogon::HttpStatusCode>(result.statusCode));
    applyCors(request, response);
    callback(response);
}

void FriendController::handleCancelRequest(const drogon::HttpRequestPtr &request,
                                           Callback &&callback,
                                           const std::string &friendshipIdValue)
{
    // 1) 인증 사용자 확인
    const auto memberId = extractCurrentMemberId(request);
    if (!memberId.has_value())
    {
        auto response =
            drogon::HttpResponse::newHttpJsonResponse(makeErrorBody("Unauthorized."));
        response->setStatusCode(drogon::k401Unauthorized);
        applyCors(request, response);
        callback(response);
        return;
    }

    // 2) 경로 변수 friendshipId 검증
    const auto friendshipId = parseFriendshipId(friendshipIdValue);
    if (!friendshipId.has_value())
    {
        auto response =
            drogon::HttpResponse::newHttpJsonResponse(makeErrorBody("Invalid friendship id."));
        response->setStatusCode(drogon::k400BadRequest);
        applyCors(request, response);
        callback(response);
        return;
    }

    // 3) 보낸 친구 요청 취소 처리
    const auto result = friendService_.cancelRequest(*memberId, *friendshipId);

    // 4) 결과 JSON 구성
    Json::Value body;
    body["ok"] = result.ok;
    body["message"] = result.message;
    // 갱신된 요청 데이터가 있으면 포함
    if (result.request.has_value())
    {
        body["request"] = buildRequestJson(*result.request);
    }

    // 5) 상태코드/CORS 적용 후 반환
    auto response = drogon::HttpResponse::newHttpJsonResponse(body);
    response->setStatusCode(static_cast<drogon::HttpStatusCode>(result.statusCode));
    applyCors(request, response);
    callback(response);
}

}  // namespace friendship

