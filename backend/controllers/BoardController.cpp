#include "BoardController.h"

#include <utility>

namespace
{

Json::Value makeErrorBody(const std::string &message)
{
    // 에러 응답은 모든 게시판 API에서 같은 JSON 구조를 사용한다.
    Json::Value body;
    body["ok"] = false;
    body["message"] = message;
    return body;
}

}  // namespace

namespace board
{

// 하나의 URL에 HTTP 메서드별 핸들러를 나눠 등록한다.
// 프론트엔드는 같은 경로를 사용하고, GET/POST/DELETE로 동작을 구분한다.
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
    // DB/서비스 계층의 DTO 필드명을 그대로 JSON 키로 노출한다.
    // location은 선택값이라 값이 있을 때만 응답에 포함한다.
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

void BoardController::applyCors(const drogon::HttpRequestPtr &request,
                                const drogon::HttpResponsePtr &response)
{
    // 개발 환경에서 프론트엔드 주소가 달라질 수 있으므로 요청 Origin을 그대로 허용한다.
    // 실제 운영 환경에서는 허용 Origin 목록으로 제한하는 편이 안전하다.
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
    // 현재 인증 구조는 세션 대신 X-Member-Id 헤더를 신뢰한다.
    // 게시글 생성/삭제처럼 작성자 식별이 필요한 요청에서만 사용한다.
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
    // DELETE 요청 본문을 파싱하지 않도록 postId는 쿼리 파라미터로 받는다.
    // 0은 유효한 게시글 ID가 아니므로 nullopt로 정리한다.
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

void BoardController::handleListPosts(const drogon::HttpRequestPtr &request,
                                      Callback &&callback)
{
    // type 파라미터는 share/exchange/all 중 하나를 기대한다.
    // 실제 허용값 검증과 DB enum 변환은 BoardService가 담당한다.
    const auto type = request->getParameter("type");
    std::optional<std::string> filter;
    if (!type.empty())
    {
        filter = type;
    }

    const auto result = boardService_.getPosts(filter);

    Json::Value body;
    body["ok"] = result.ok;
    body["message"] = result.message;

    Json::Value posts(Json::arrayValue);
    for (const auto &post : result.posts)
    {
        posts.append(buildPostJson(post));
    }
    body["posts"] = posts;

    auto response = drogon::HttpResponse::newHttpJsonResponse(body);
    response->setStatusCode(static_cast<drogon::HttpStatusCode>(result.statusCode));
    applyCors(request, response);
    callback(response);
}

void BoardController::handleCreatePost(const drogon::HttpRequestPtr &request,
                                       Callback &&callback)
{
    // 게시글 생성은 작성자 식별이 필수이므로 멤버 ID를 먼저 확인한다.
    // 이후 JSON 본문을 DTO로 옮기고 서비스 계층에서 상세 검증을 수행한다.
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
    // 삭제는 소프트 삭제로 처리되며, 본인 게시글 여부는 서비스에서 확인한다.
    // 컨트롤러는 인증 헤더와 postId 파라미터가 있는지만 검사한다.
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
