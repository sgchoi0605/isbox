#include "BoardController.h"

#include <charconv>
#include <limits>
#include <system_error>
#include <utility>

namespace
{

Json::Value makeErrorBody(const std::string &message)
{
    Json::Value body;
    body["ok"] = false;
    body["message"] = message;
    return body;
}

}  // namespace

namespace board
{

void BoardController::registerHandlers()
{
    auto &app = drogon::app();

    app.registerHandler(
        "/api/board/posts",
        [this](const drogon::HttpRequestPtr &request, Callback &&callback) {
            handleListPosts(request, std::move(callback));
        },
        {drogon::Get});

    app.registerHandler(
        "/api/board/posts",
        [this](const drogon::HttpRequestPtr &request, Callback &&callback) {
            handleCreatePost(request, std::move(callback));
        },
        {drogon::Post});

    app.registerHandler(
        "/api/board/posts",
        [this](const drogon::HttpRequestPtr &request, Callback &&callback) {
            handleDeletePost(request, std::move(callback));
        },
        {drogon::Delete});
}

Json::Value BoardController::buildPostJson(const BoardPostDTO &post)
{
    Json::Value postJson;

    postJson["postId"] = static_cast<Json::UInt64>(post.postId);
    postJson["memberId"] = static_cast<Json::UInt64>(post.memberId);
    postJson["type"] = post.type;
    postJson["title"] = post.title;
    postJson["content"] = post.content;

    if (post.location.has_value())
    {
        postJson["location"] = *post.location;
    }

    postJson["authorName"] = post.authorName;
    postJson["status"] = post.status;
    postJson["createdAt"] = post.createdAt;
    return postJson;
}

Json::Value BoardController::buildPaginationJson(const BoardPaginationDTO &pagination)
{
    Json::Value paginationJson;
    paginationJson["page"] = pagination.page;
    paginationJson["size"] = pagination.size;
    paginationJson["totalItems"] = static_cast<Json::UInt64>(pagination.totalItems);
    paginationJson["totalPages"] = pagination.totalPages;
    paginationJson["hasPrev"] = pagination.hasPrev;
    paginationJson["hasNext"] = pagination.hasNext;
    return paginationJson;
}

void BoardController::applyCors(const drogon::HttpRequestPtr &request,
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
    response->addHeader("Access-Control-Allow-Headers",
                        "Content-Type, X-Member-Id");
    response->addHeader("Access-Control-Allow-Methods",
                        "GET, POST, DELETE, OPTIONS");
}

std::optional<std::uint64_t> BoardController::extractMemberIdHeader(
    const drogon::HttpRequestPtr &request)
{
    auto memberIdValue = request->getHeader("X-Member-Id");
    if (memberIdValue.empty())
    {
        memberIdValue = request->getHeader("x-member-id");
    }

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

std::optional<std::uint64_t> BoardController::extractPostIdParameter(
    const drogon::HttpRequestPtr &request)
{
    const auto postIdValue = request->getParameter("postId");
    if (postIdValue.empty())
    {
        return std::nullopt;
    }

    try
    {
        const auto parsed = std::stoull(postIdValue);
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

std::optional<std::uint32_t> BoardController::parsePositiveIntegerParameter(
    const std::string &value,
    std::uint32_t minValue,
    std::uint32_t maxValue)
{
    if (value.empty())
    {
        return std::nullopt;
    }

    std::uint64_t parsed = 0;
    const auto *begin = value.data();
    const auto *end = value.data() + value.size();
    const auto [ptr, ec] = std::from_chars(begin, end, parsed);

    if (ec != std::errc() || ptr != end)
    {
        return std::nullopt;
    }

    if (parsed < minValue || parsed > maxValue ||
        parsed > (std::numeric_limits<std::uint32_t>::max)())
    {
        return std::nullopt;
    }

    return static_cast<std::uint32_t>(parsed);
}

void BoardController::handleListPosts(const drogon::HttpRequestPtr &request,
                                      Callback &&callback)
{
    BoardListRequestDTO listRequest;

    const auto type = request->getParameter("type");
    if (!type.empty())
    {
        listRequest.type = type;
    }

    const auto search = request->getParameter("search");
    if (!search.empty())
    {
        listRequest.search = search;
    }

    const auto pageValue = request->getParameter("page");
    if (!pageValue.empty())
    {
        const auto page = parsePositiveIntegerParameter(
            pageValue,
            1,
            (std::numeric_limits<std::uint32_t>::max)());
        if (!page.has_value())
        {
            auto response =
                drogon::HttpResponse::newHttpJsonResponse(makeErrorBody("Invalid page parameter."));
            response->setStatusCode(drogon::k400BadRequest);
            applyCors(request, response);
            callback(response);
            return;
        }
        listRequest.page = *page;
    }

    const auto sizeValue = request->getParameter("size");
    if (!sizeValue.empty())
    {
        const auto size = parsePositiveIntegerParameter(sizeValue, 1, 50);
        if (!size.has_value())
        {
            auto response =
                drogon::HttpResponse::newHttpJsonResponse(makeErrorBody("Invalid size parameter."));
            response->setStatusCode(drogon::k400BadRequest);
            applyCors(request, response);
            callback(response);
            return;
        }
        listRequest.size = *size;
    }

    const auto result = boardService_.getPosts(listRequest);

    Json::Value body;
    body["ok"] = result.ok;
    body["message"] = result.message;

    Json::Value posts(Json::arrayValue);
    for (const auto &post : result.posts)
    {
        posts.append(buildPostJson(post));
    }

    body["posts"] = posts;
    body["pagination"] = buildPaginationJson(result.pagination);

    auto response = drogon::HttpResponse::newHttpJsonResponse(body);
    response->setStatusCode(static_cast<drogon::HttpStatusCode>(result.statusCode));

    applyCors(request, response);
    callback(response);
}

void BoardController::handleCreatePost(const drogon::HttpRequestPtr &request,
                                       Callback &&callback)
{
    const auto memberId = extractMemberIdHeader(request);
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

    CreateBoardPostRequestDTO dto;
    dto.type = json->get("type", "").asString();
    dto.title = json->get("title", "").asString();
    dto.content = json->get("content", "").asString();

    const auto location = json->get("location", "").asString();
    if (!location.empty())
    {
        dto.location = location;
    }

    const auto result = boardService_.createPost(*memberId, dto);

    Json::Value body;
    body["ok"] = result.ok;
    body["message"] = result.message;

    if (result.post.has_value())
    {
        body["post"] = buildPostJson(*result.post);
    }

    auto response = drogon::HttpResponse::newHttpJsonResponse(body);
    response->setStatusCode(static_cast<drogon::HttpStatusCode>(result.statusCode));

    applyCors(request, response);
    callback(response);
}

void BoardController::handleDeletePost(const drogon::HttpRequestPtr &request,
                                       Callback &&callback)
{
    const auto memberId = extractMemberIdHeader(request);
    if (!memberId.has_value())
    {
        auto response =
            drogon::HttpResponse::newHttpJsonResponse(makeErrorBody("Unauthorized."));
        response->setStatusCode(drogon::k401Unauthorized);
        applyCors(request, response);
        callback(response);
        return;
    }

    const auto postId = extractPostIdParameter(request);
    if (!postId.has_value())
    {
        auto response =
            drogon::HttpResponse::newHttpJsonResponse(makeErrorBody("Post id is required."));
        response->setStatusCode(drogon::k400BadRequest);
        applyCors(request, response);
        callback(response);
        return;
    }

    const auto result = boardService_.deletePost(*memberId, *postId);

    Json::Value body;
    body["ok"] = result.ok;
    body["message"] = result.message;

    auto response = drogon::HttpResponse::newHttpJsonResponse(body);
    response->setStatusCode(static_cast<drogon::HttpStatusCode>(result.statusCode));

    applyCors(request, response);
    callback(response);
}

}  // namespace board
