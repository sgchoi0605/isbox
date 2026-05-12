#include "FriendController.h"

#include <utility>

namespace
{

constexpr const char *kSessionCookieName = "isbox_session";

Json::Value makeErrorBody(const std::string &message)
{
    Json::Value body;
    body["ok"] = false;
    body["message"] = message;
    return body;
}

}  // namespace

namespace friendship
{

void FriendController::registerHandlers()
{
    auto &app = drogon::app();

    app.registerHandler(
        "/api/friends/search",
        [this](const drogon::HttpRequestPtr &request, Callback &&callback) {
            handleSearchMember(request, std::move(callback));
        },
        {drogon::Get});

    app.registerHandler(
        "/api/friends/requests",
        [this](const drogon::HttpRequestPtr &request, Callback &&callback) {
            handleSendRequest(request, std::move(callback));
        },
        {drogon::Post});

    app.registerHandler(
        "/api/friends",
        [this](const drogon::HttpRequestPtr &request, Callback &&callback) {
            handleListFriends(request, std::move(callback));
        },
        {drogon::Get});

    app.registerHandler(
        "/api/friends/requests/incoming",
        [this](const drogon::HttpRequestPtr &request, Callback &&callback) {
            handleListIncomingRequests(request, std::move(callback));
        },
        {drogon::Get});

    app.registerHandler(
        "/api/friends/requests/outgoing",
        [this](const drogon::HttpRequestPtr &request, Callback &&callback) {
            handleListOutgoingRequests(request, std::move(callback));
        },
        {drogon::Get});

    app.registerHandler(
        "/api/friends/requests/{1}/accept",
        [this](const drogon::HttpRequestPtr &request,
               Callback &&callback,
               const std::string &friendshipIdValue) {
            handleAcceptRequest(request, std::move(callback), friendshipIdValue);
        },
        {drogon::Post});

    app.registerHandler(
        "/api/friends/requests/{1}/reject",
        [this](const drogon::HttpRequestPtr &request,
               Callback &&callback,
               const std::string &friendshipIdValue) {
            handleRejectRequest(request, std::move(callback), friendshipIdValue);
        },
        {drogon::Post});

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
    Json::Value out;
    out["memberId"] = static_cast<Json::UInt64>(member.memberId);
    out["name"] = member.name;
    out["email"] = member.email;
    out["status"] = member.status;
    return out;
}

Json::Value FriendController::buildFriendJson(const FriendDTO &friendItem)
{
    Json::Value out;
    out["friendshipId"] = static_cast<Json::UInt64>(friendItem.friendshipId);
    out["memberId"] = static_cast<Json::UInt64>(friendItem.memberId);
    out["name"] = friendItem.name;
    out["email"] = friendItem.email;
    out["status"] = friendItem.status;
    out["requestedAt"] = friendItem.requestedAt;
    if (friendItem.respondedAt.has_value())
    {
        out["respondedAt"] = *friendItem.respondedAt;
    }
    return out;
}

Json::Value FriendController::buildRequestJson(const FriendRequestDTO &request)
{
    Json::Value out;
    out["friendshipId"] = static_cast<Json::UInt64>(request.friendshipId);
    out["memberId"] = static_cast<Json::UInt64>(request.memberId);
    out["name"] = request.name;
    out["email"] = request.email;
    out["status"] = request.status;
    out["requestedAt"] = request.requestedAt;
    if (request.respondedAt.has_value())
    {
        out["respondedAt"] = *request.respondedAt;
    }
    return out;
}

void FriendController::applyCors(const drogon::HttpRequestPtr &request,
                                 const drogon::HttpResponsePtr &response)
{
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
    response->addHeader("Access-Control-Allow-Methods",
                        "GET, POST, PUT, OPTIONS");
}

std::string FriendController::extractSessionToken(
    const drogon::HttpRequestPtr &request)
{
    return request->getCookie(kSessionCookieName);
}

std::optional<std::uint64_t> FriendController::parseFriendshipId(
    const std::string &friendshipIdValue)
{
    if (friendshipIdValue.empty())
    {
        return std::nullopt;
    }

    try
    {
        const auto parsed = std::stoull(friendshipIdValue);
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

std::optional<std::uint64_t> FriendController::extractCurrentMemberId(
    const drogon::HttpRequestPtr &request)
{
    return memberService_.getCurrentMemberId(extractSessionToken(request));
}

void FriendController::handleSearchMember(const drogon::HttpRequestPtr &request,
                                          Callback &&callback)
{
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

    const auto result =
        friendService_.searchMemberByEmail(*memberId, request->getParameter("email"));

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

void FriendController::handleSendRequest(const drogon::HttpRequestPtr &request,
                                         Callback &&callback)
{
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

    const auto result =
        friendService_.sendFriendRequest(*memberId, json->get("email", "").asString());

    Json::Value body;
    body["ok"] = result.ok;
    body["message"] = result.message;
    if (result.request.has_value())
    {
        body["request"] = buildRequestJson(*result.request);
    }

    auto response = drogon::HttpResponse::newHttpJsonResponse(body);
    response->setStatusCode(static_cast<drogon::HttpStatusCode>(result.statusCode));
    applyCors(request, response);
    callback(response);
}

void FriendController::handleListFriends(const drogon::HttpRequestPtr &request,
                                         Callback &&callback)
{
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

    const auto result = friendService_.listAcceptedFriends(*memberId);

    Json::Value body;
    body["ok"] = result.ok;
    body["message"] = result.message;

    Json::Value friendsJson(Json::arrayValue);
    for (const auto &friendItem : result.friends)
    {
        friendsJson.append(buildFriendJson(friendItem));
    }
    body["friends"] = friendsJson;

    auto response = drogon::HttpResponse::newHttpJsonResponse(body);
    response->setStatusCode(static_cast<drogon::HttpStatusCode>(result.statusCode));
    applyCors(request, response);
    callback(response);
}

void FriendController::handleListIncomingRequests(
    const drogon::HttpRequestPtr &request,
    Callback &&callback)
{
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

    const auto result = friendService_.listIncomingRequests(*memberId);

    Json::Value body;
    body["ok"] = result.ok;
    body["message"] = result.message;

    Json::Value requestsJson(Json::arrayValue);
    for (const auto &item : result.requests)
    {
        requestsJson.append(buildRequestJson(item));
    }
    body["requests"] = requestsJson;

    auto response = drogon::HttpResponse::newHttpJsonResponse(body);
    response->setStatusCode(static_cast<drogon::HttpStatusCode>(result.statusCode));
    applyCors(request, response);
    callback(response);
}

void FriendController::handleListOutgoingRequests(
    const drogon::HttpRequestPtr &request,
    Callback &&callback)
{
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

    const auto result = friendService_.listOutgoingRequests(*memberId);

    Json::Value body;
    body["ok"] = result.ok;
    body["message"] = result.message;

    Json::Value requestsJson(Json::arrayValue);
    for (const auto &item : result.requests)
    {
        requestsJson.append(buildRequestJson(item));
    }
    body["requests"] = requestsJson;

    auto response = drogon::HttpResponse::newHttpJsonResponse(body);
    response->setStatusCode(static_cast<drogon::HttpStatusCode>(result.statusCode));
    applyCors(request, response);
    callback(response);
}

void FriendController::handleAcceptRequest(const drogon::HttpRequestPtr &request,
                                           Callback &&callback,
                                           const std::string &friendshipIdValue)
{
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

    const auto result = friendService_.acceptRequest(*memberId, *friendshipId);

    Json::Value body;
    body["ok"] = result.ok;
    body["message"] = result.message;
    if (result.request.has_value())
    {
        body["request"] = buildRequestJson(*result.request);
    }

    auto response = drogon::HttpResponse::newHttpJsonResponse(body);
    response->setStatusCode(static_cast<drogon::HttpStatusCode>(result.statusCode));
    applyCors(request, response);
    callback(response);
}

void FriendController::handleRejectRequest(const drogon::HttpRequestPtr &request,
                                           Callback &&callback,
                                           const std::string &friendshipIdValue)
{
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

    const auto result = friendService_.rejectRequest(*memberId, *friendshipId);

    Json::Value body;
    body["ok"] = result.ok;
    body["message"] = result.message;
    if (result.request.has_value())
    {
        body["request"] = buildRequestJson(*result.request);
    }

    auto response = drogon::HttpResponse::newHttpJsonResponse(body);
    response->setStatusCode(static_cast<drogon::HttpStatusCode>(result.statusCode));
    applyCors(request, response);
    callback(response);
}

void FriendController::handleCancelRequest(const drogon::HttpRequestPtr &request,
                                           Callback &&callback,
                                           const std::string &friendshipIdValue)
{
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

    const auto result = friendService_.cancelRequest(*memberId, *friendshipId);

    Json::Value body;
    body["ok"] = result.ok;
    body["message"] = result.message;
    if (result.request.has_value())
    {
        body["request"] = buildRequestJson(*result.request);
    }

    auto response = drogon::HttpResponse::newHttpJsonResponse(body);
    response->setStatusCode(static_cast<drogon::HttpStatusCode>(result.statusCode));
    applyCors(request, response);
    callback(response);
}

}  // namespace friendship

