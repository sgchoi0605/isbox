#include "BoardController.h"

#include <utility>

namespace
{

// 공통 에러 응답 JSON 본문을 생성한다.
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

// 게시판 API 엔드포인트를 HTTP 메서드별로 등록한다.
void BoardController::registerHandlers()
{
    // 전역 Drogon 앱 인스턴스를 가져온다.
    auto &app = drogon::app();

    // GET /api/board/posts: 게시글 목록 조회
    app.registerHandler(
        "/api/board/posts",
        [this](const drogon::HttpRequestPtr &request, Callback &&callback) {
            handleListPosts(request, std::move(callback));
        },
        {drogon::Get});

    // POST /api/board/posts: 게시글 등록
    app.registerHandler(
        "/api/board/posts",
        [this](const drogon::HttpRequestPtr &request, Callback &&callback) {
            handleCreatePost(request, std::move(callback));
        },
        {drogon::Post});

    // DELETE /api/board/posts: 게시글 삭제
    app.registerHandler(
        "/api/board/posts",
        [this](const drogon::HttpRequestPtr &request, Callback &&callback) {
            handleDeletePost(request, std::move(callback));
        },
        {drogon::Delete});
}

// 게시글 DTO를 응답용 JSON 객체로 변환한다.
Json::Value BoardController::buildPostJson(const BoardPostDTO &post)
{
    Json::Value postJson;

    // 숫자 ID는 JSON UInt64 타입으로 명시 변환한다.
    postJson["postId"] = static_cast<Json::UInt64>(post.postId);
    postJson["memberId"] = static_cast<Json::UInt64>(post.memberId);

    // 기본 텍스트 필드를 그대로 매핑한다.
    postJson["type"] = post.type;
    postJson["title"] = post.title;
    postJson["content"] = post.content;

    // 선택 필드는 값이 있을 때만 내려준다.
    if (post.location.has_value())
    {
        postJson["location"] = *post.location;
    }

    // 작성자/상태/생성시간 정보를 포함한다.
    postJson["authorName"] = post.authorName;
    postJson["status"] = post.status;
    postJson["createdAt"] = post.createdAt;
    return postJson;
}

// 요청 Origin 기준으로 CORS 응답 헤더를 추가한다.
void BoardController::applyCors(const drogon::HttpRequestPtr &request,
                                const drogon::HttpResponsePtr &response)
{
    // 브라우저가 보낸 Origin 헤더를 읽는다(대소문자 모두 대응).
    std::string origin = request->getHeader("Origin");
    if (origin.empty())
    {
        origin = request->getHeader("origin");
    }

    // Origin이 있으면 해당 Origin만 허용하고 Vary를 설정한다.
    if (!origin.empty())
    {
        response->addHeader("Access-Control-Allow-Origin", origin);
        response->addHeader("Vary", "Origin");
    }

    // 인증 쿠키/헤더 사용을 허용하고 허용 메서드/헤더를 명시한다.
    response->addHeader("Access-Control-Allow-Credentials", "true");
    response->addHeader("Access-Control-Allow-Headers",
                        "Content-Type, X-Member-Id");
    response->addHeader("Access-Control-Allow-Methods",
                        "GET, POST, DELETE, OPTIONS");
}

// X-Member-Id 헤더를 안전하게 숫자 ID로 파싱한다.
std::optional<std::uint64_t> BoardController::extractMemberIdHeader(
    const drogon::HttpRequestPtr &request)
{
    // 표준/소문자 헤더명을 모두 시도한다.
    auto memberIdValue = request->getHeader("X-Member-Id");
    if (memberIdValue.empty())
    {
        memberIdValue = request->getHeader("x-member-id");
    }

    // 헤더가 없으면 인증 정보가 없는 것으로 처리한다.
    if (memberIdValue.empty())
    {
        return std::nullopt;
    }

    try
    {
        // 양의 정수 ID인지 확인한다.
        const auto parsed = std::stoull(memberIdValue);
        if (parsed == 0)
        {
            return std::nullopt;
        }
        return parsed;
    }
    catch (const std::exception &)
    {
        // 숫자 파싱 실패 시 잘못된 인증 헤더로 처리한다.
        return std::nullopt;
    }
}

// 쿼리 파라미터 postId를 안전하게 숫자 ID로 파싱한다.
std::optional<std::uint64_t> BoardController::extractPostIdParameter(
    const drogon::HttpRequestPtr &request)
{
    // DELETE 요청에서 postId 쿼리 파라미터를 읽는다.
    const auto postIdValue = request->getParameter("postId");
    if (postIdValue.empty())
    {
        return std::nullopt;
    }

    try
    {
        // 0은 유효한 게시글 ID가 아니므로 거부한다.
        const auto parsed = std::stoull(postIdValue);
        if (parsed == 0)
        {
            return std::nullopt;
        }
        return parsed;
    }
    catch (const std::exception &)
    {
        // 숫자 파싱 실패 시 잘못된 요청으로 처리한다.
        return std::nullopt;
    }
}

// 게시글 목록 조회 요청을 처리한다.
void BoardController::handleListPosts(const drogon::HttpRequestPtr &request,
                                      Callback &&callback)
{
    // 선택 필터(type)가 있으면 서비스로 전달한다.
    const auto type = request->getParameter("type");
    std::optional<std::string> filter;
    if (!type.empty())
    {
        filter = type;
    }

    // 서비스 계층에서 목록 조회를 수행한다.
    const auto result = boardService_.getPosts(filter);

    // 공통 응답 필드를 구성한다.
    Json::Value body;
    body["ok"] = result.ok;
    body["message"] = result.message;

    // 조회된 게시글 목록을 JSON 배열로 직렬화한다.
    Json::Value posts(Json::arrayValue);
    for (const auto &post : result.posts)
    {
        posts.append(buildPostJson(post));
    }
    body["posts"] = posts;

    // JSON 응답을 만들고 서비스 결과 상태코드를 반영한다.
    auto response = drogon::HttpResponse::newHttpJsonResponse(body);
    response->setStatusCode(static_cast<drogon::HttpStatusCode>(result.statusCode));

    // CORS 헤더를 붙여 브라우저 호출을 허용한 뒤 응답한다.
    applyCors(request, response);
    callback(response);
}

// 게시글 생성 요청을 처리한다.
void BoardController::handleCreatePost(const drogon::HttpRequestPtr &request,
                                       Callback &&callback)
{
    // 작성자 식별을 위해 멤버 ID 헤더를 확인한다.
    const auto memberId = extractMemberIdHeader(request);
    if (!memberId.has_value())
    {
        // 인증 정보가 없으면 401을 반환한다.
        auto response =
            drogon::HttpResponse::newHttpJsonResponse(makeErrorBody("Unauthorized."));
        response->setStatusCode(drogon::k401Unauthorized);
        applyCors(request, response);
        callback(response);
        return;
    }

    // 요청 본문이 유효한 JSON인지 확인한다.
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

    // JSON 본문을 게시글 생성 DTO로 변환한다.
    CreateBoardPostRequestDTO dto;
    dto.type = json->get("type", "").asString();
    dto.title = json->get("title", "").asString();
    dto.content = json->get("content", "").asString();

    // location은 선택 필드이므로 비어있지 않을 때만 설정한다.
    const auto location = json->get("location", "").asString();
    if (!location.empty())
    {
        dto.location = location;
    }

    // 생성 요청을 서비스 계층으로 위임한다.
    const auto result = boardService_.createPost(*memberId, dto);

    // 공통 응답 필드를 구성한다.
    Json::Value body;
    body["ok"] = result.ok;
    body["message"] = result.message;

    // 생성된 게시글이 있으면 응답에 포함한다.
    if (result.post.has_value())
    {
        body["post"] = buildPostJson(*result.post);
    }

    // JSON 응답 생성 후 상태코드를 반영한다.
    auto response = drogon::HttpResponse::newHttpJsonResponse(body);
    response->setStatusCode(static_cast<drogon::HttpStatusCode>(result.statusCode));

    // CORS 헤더를 적용하고 응답한다.
    applyCors(request, response);
    callback(response);
}

// 게시글 삭제 요청을 처리한다.
void BoardController::handleDeletePost(const drogon::HttpRequestPtr &request,
                                       Callback &&callback)
{
    // 삭제 권한 검증을 위해 멤버 ID를 먼저 확인한다.
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

    // 삭제 대상 게시글 ID를 쿼리 파라미터에서 추출한다.
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

    // 실제 삭제 로직은 서비스 계층에서 수행한다.
    const auto result = boardService_.deletePost(*memberId, *postId);

    // 공통 응답 본문을 구성한다.
    Json::Value body;
    body["ok"] = result.ok;
    body["message"] = result.message;

    // JSON 응답 생성 후 상태코드를 반영한다.
    auto response = drogon::HttpResponse::newHttpJsonResponse(body);
    response->setStatusCode(static_cast<drogon::HttpStatusCode>(result.statusCode));

    // CORS 헤더 적용 후 응답 콜백을 호출한다.
    applyCors(request, response);
    callback(response);
}

}  // namespace board
