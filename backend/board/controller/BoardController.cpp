/*
 * 파일 역할:
 * - BoardController의 HTTP 핸들러 구현체를 제공한다.
 * - 요청 파라미터/헤더/JSON을 DTO로 변환하고 기본 형식 검증을 수행한다.
 * - 서비스 결과를 공통 JSON 응답 스키마로 직렬화하고 CORS 헤더를 일관되게 적용한다.
 */
#include "board/controller/BoardController.h"

#include <charconv>
#include <limits>
#include <system_error>
#include <utility>

namespace
{

// 실패 응답에서 공통으로 쓰는 최소 JSON 본문을 만든다.
// 모든 에러 응답이 동일한 `{ ok, message }` 형태를 유지하도록 맞춘다.
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
    // 전역 Drogon 앱 인스턴스에 게시판 라우트를 연결한다.
    // 같은 URL 경로에 메서드별(GET/POST/DELETE) 핸들러를 분기 등록한다.
    auto &app = drogon::app();

    // 게시글 목록 조회: GET /api/board/posts
    app.registerHandler(
        "/api/board/posts",
        [this](const drogon::HttpRequestPtr &request, Callback &&callback) {
            handleListPosts(request, std::move(callback));
        },
        {drogon::Get});

    // 게시글 생성: POST /api/board/posts
    app.registerHandler(
        "/api/board/posts",
        [this](const drogon::HttpRequestPtr &request, Callback &&callback) {
            handleCreatePost(request, std::move(callback));
        },
        {drogon::Post});

    // 게시글 삭제: DELETE /api/board/posts?postId=...
    app.registerHandler(
        "/api/board/posts",
        [this](const drogon::HttpRequestPtr &request, Callback &&callback) {
            handleDeletePost(request, std::move(callback));
        },
        {drogon::Delete});
}

Json::Value BoardController::buildPostJson(const BoardPostDTO &post)
{
    // DTO 필드를 API 응답 스키마에 맞춰 직렬화한다.
    // 숫자형 식별자는 UInt64로 유지해 직렬화 과정에서 정보 손실을 피한다.
    Json::Value postJson;

    postJson["postId"] = static_cast<Json::UInt64>(post.postId);
    postJson["memberId"] = static_cast<Json::UInt64>(post.memberId);
    postJson["type"] = post.type;
    postJson["title"] = post.title;
    postJson["content"] = post.content;

    if (post.location.has_value())
    {
        // 위치 정보는 값이 있을 때만 내려준다.
        // 클라이언트가 optional 필드를 자연스럽게 처리하도록 누락 방식으로 전달한다.
        postJson["location"] = *post.location;
    }

    postJson["authorName"] = post.authorName;
    postJson["status"] = post.status;
    postJson["createdAt"] = post.createdAt;
    return postJson;
}

Json::Value BoardController::buildPaginationJson(const BoardPaginationDTO &pagination)
{
    // 페이징 메타데이터를 응답 JSON으로 만든다.
    Json::Value paginationJson;
    paginationJson["page"] = pagination.page;
    paginationJson["size"] = pagination.size;
    paginationJson["totalItems"] = static_cast<Json::UInt64>(pagination.totalItems);
    paginationJson["totalPages"] = pagination.totalPages;
    paginationJson["hasPrev"] = pagination.hasPrev;
    paginationJson["hasNext"] = pagination.hasNext;
    return paginationJson;
}

void BoardController::sendJson(const drogon::HttpRequestPtr &request,
                               const Callback &callback,
                               const Json::Value &body,
                               drogon::HttpStatusCode statusCode)
{
    // CORS 포함 공통 JSON 응답 경로.
    // 모든 핸들러의 최종 응답 출구를 단일 함수로 모아 정책 일관성을 보장한다.
    auto response = drogon::HttpResponse::newHttpJsonResponse(body);
    response->setStatusCode(statusCode);
    applyCors(request, response);
    callback(response);
}

void BoardController::sendError(const drogon::HttpRequestPtr &request,
                                const Callback &callback,
                                drogon::HttpStatusCode statusCode,
                                const std::string &message)
{
    // 오류 응답은 공통 본문 생성 + 공통 JSON 응답 경로를 사용한다.
    sendJson(request, callback, makeErrorBody(message), statusCode);
}

void BoardController::applyCors(const drogon::HttpRequestPtr &request,
                                const drogon::HttpResponsePtr &response)
{
    // 대소문자 혼용 클라이언트를 위해 Origin 헤더를 모두 시도한다.
    std::string origin = request->getHeader("Origin");
    if (origin.empty())
    {
        origin = request->getHeader("origin");
    }

    if (!origin.empty())
    {
        // 요청 Origin을 그대로 반영해 credential 포함 CORS를 허용한다.
        // Vary: Origin을 함께 내려 캐시가 다른 Origin 응답을 재사용하지 않게 한다.
        response->addHeader("Access-Control-Allow-Origin", origin);
        response->addHeader("Vary", "Origin");
    }

    // 게시판 API에서 필요한 기본 CORS 정책을 설정한다.
    response->addHeader("Access-Control-Allow-Credentials", "true");
    response->addHeader("Access-Control-Allow-Headers",
                        "Content-Type, X-Member-Id");
    response->addHeader("Access-Control-Allow-Methods",
                        "GET, POST, DELETE, OPTIONS");
}

std::optional<std::uint64_t> BoardController::extractMemberIdHeader(
    const drogon::HttpRequestPtr &request)
{
    // 인증용 회원 ID 헤더를 읽는다.
    // 일부 클라이언트 구현 차이를 감안해 헤더 키의 대소문자 변형을 모두 허용한다.
    auto memberIdValue = request->getHeader("X-Member-Id");
    if (memberIdValue.empty())
    {
        memberIdValue = request->getHeader("x-member-id");
    }

    if (memberIdValue.empty())
    {
        // 헤더가 없으면 인증 정보가 없는 것으로 본다.
        return std::nullopt;
    }

    try
    {
        // 0은 유효한 사용자 ID로 취급하지 않는다.
        // 숫자 변환 실패나 음수 입력은 예외/검증 단계에서 자동 탈락한다.
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
    // 삭제 대상 게시글 ID를 쿼리 파라미터에서 읽는다.
    const auto postIdValue = request->getParameter("postId");
    if (postIdValue.empty())
    {
        return std::nullopt;
    }

    try
    {
        // 0은 유효한 게시글 ID로 취급하지 않는다.
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
    // 값이 없으면 "파라미터 미지정"으로 처리한다.
    // 호출부에서 기본값을 유지할 수 있도록 nullopt를 반환한다.
    if (value.empty())
    {
        return std::nullopt;
    }

    // 예외 없는 숫자 파싱으로 입력 형식을 검증한다.
    // from_chars는 로케일/예외 비용 없이 전체 문자열 숫자 여부를 확인할 수 있다.
    std::uint64_t parsed = 0;
    const auto *begin = value.data();
    const auto *end = value.data() + value.size();
    const auto [ptr, ec] = std::from_chars(begin, end, parsed);

    if (ec != std::errc() || ptr != end)
    {
        // 문자열 전체가 숫자가 아니면 실패다.
        return std::nullopt;
    }

    if (parsed < minValue || parsed > maxValue ||
        parsed > (std::numeric_limits<std::uint32_t>::max)())
    {
        // 허용 범위를 벗어나면 실패다.
        // 과도한 page/size 입력을 조기에 차단해 불필요한 부하를 막는다.
        return std::nullopt;
    }

    return static_cast<std::uint32_t>(parsed);
}

void BoardController::handleListPosts(const drogon::HttpRequestPtr &request,
                                      Callback &&callback)
{
    // 쿼리 문자열을 서비스 요청 DTO로 변환한다.
    // 컨트롤러는 형식 검증(숫자 파싱/범위)까지만 수행하고 도메인 검증은 서비스에 위임한다.
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
        // page는 1 이상 양의 정수만 허용한다.
        const auto page = parsePositiveIntegerParameter(
            pageValue,
            1,
            (std::numeric_limits<std::uint32_t>::max)());
        if (!page.has_value())
        {
            sendError(request, callback, drogon::k400BadRequest, "Invalid page parameter.");
            return;
        }
        listRequest.page = *page;
    }

    const auto sizeValue = request->getParameter("size");
    if (!sizeValue.empty())
    {
        // size는 과도한 조회를 막기 위해 최대 50으로 제한한다.
        // 서비스 계층에서도 동일 제한을 재검증해 이중 방어한다.
        const auto size = parsePositiveIntegerParameter(sizeValue, 1, 50);
        if (!size.has_value())
        {
            sendError(request, callback, drogon::k400BadRequest, "Invalid size parameter.");
            return;
        }
        listRequest.size = *size;
    }

    // 검증된 요청 DTO로 목록 조회 비즈니스 로직을 호출한다.
    const auto result = boardService_.getPosts(listRequest);

    // 서비스 결과를 API 응답 본문으로 재구성한다.
    // 컨트롤러는 응답 스키마 조립만 담당하며, 비즈니스 판단은 결과를 그대로 반영한다.
    Json::Value body;
    body["ok"] = result.ok;
    body["message"] = result.message;

    Json::Value posts(Json::arrayValue);
    for (const auto &post : result.posts)
    {
        // 게시글 배열의 각 원소를 DTO -> JSON으로 변환한다.
        posts.append(buildPostJson(post));
    }

    body["posts"] = posts;
    body["pagination"] = buildPaginationJson(result.pagination);

    sendJson(request,
             callback,
             body,
             static_cast<drogon::HttpStatusCode>(result.statusCode));
}

void BoardController::handleCreatePost(const drogon::HttpRequestPtr &request,
                                       Callback &&callback)
{
    // 인증 헤더가 없거나 잘못되면 생성 요청을 거절한다.
    // 인증 실패는 항상 401로 반환해 클라이언트의 분기 처리를 단순화한다.
    const auto memberId = extractMemberIdHeader(request);
    if (!memberId.has_value())
    {
        sendError(request, callback, drogon::k401Unauthorized, "Unauthorized.");
        return;
    }

    const auto json = request->getJsonObject();
    if (!json)
    {
        // JSON 본문이 아니면 요청 형식 오류다.
        sendError(request, callback, drogon::k400BadRequest, "Invalid JSON body.");
        return;
    }

    // JSON payload를 서비스 요청 DTO로 옮긴다.
    // 필수값/길이/허용 타입 같은 도메인 유효성은 서비스 계층에서 최종 검증한다.
    CreateBoardPostRequestDTO dto;
    dto.type = json->get("type", "").asString();
    dto.title = json->get("title", "").asString();
    dto.content = json->get("content", "").asString();

    const auto location = json->get("location", "").asString();
    if (!location.empty())
    {
        // 빈 문자열이 아닌 위치만 선택적으로 저장한다.
        dto.location = location;
    }

    // 서비스 계층에서 검증/저장 후 결과를 반환받는다.
    const auto result = boardService_.createPost(*memberId, dto);

    Json::Value body;
    body["ok"] = result.ok;
    body["message"] = result.message;

    if (result.post.has_value())
    {
        // 생성 성공 시 생성된 게시글 상세를 함께 반환한다.
        body["post"] = buildPostJson(*result.post);
    }

    sendJson(request,
             callback,
             body,
             static_cast<drogon::HttpStatusCode>(result.statusCode));
}

void BoardController::handleDeletePost(const drogon::HttpRequestPtr &request,
                                       Callback &&callback)
{
    // 삭제도 인증이 필요하다.
    const auto memberId = extractMemberIdHeader(request);
    if (!memberId.has_value())
    {
        sendError(request, callback, drogon::k401Unauthorized, "Unauthorized.");
        return;
    }

    const auto postId = extractPostIdParameter(request);
    if (!postId.has_value())
    {
        // 대상 ID가 없으면 삭제를 진행할 수 없다.
        sendError(request, callback, drogon::k400BadRequest, "Post id is required.");
        return;
    }

    // 소유자/존재 여부 검증과 실제 삭제는 서비스 계층이 담당한다.
    // 컨트롤러는 삭제 대상 ID 형식과 인증 헤더 존재만 확인한다.
    const auto result = boardService_.deletePost(*memberId, *postId);

    Json::Value body;
    body["ok"] = result.ok;
    body["message"] = result.message;

    sendJson(request,
             callback,
             body,
             static_cast<drogon::HttpStatusCode>(result.statusCode));
}

}  // namespace board
