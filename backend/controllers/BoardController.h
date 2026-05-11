#pragma once

#include <drogon/HttpTypes.h>
#include <drogon/drogon.h>

#include <functional>
#include <optional>
#include <string>

#include "../services/BoardService.h"

namespace board
{

// 게시판 HTTP 계층을 담당한다.
// 요청에서 헤더/쿼리/JSON 값을 꺼내 DTO로 변환하고, 실제 검증과 저장은 BoardService에 위임한다.
class BoardController
{
  public:
    BoardController() = default;

    // Drogon 애플리케이션에 게시글 조회/생성/삭제 라우트를 등록한다.
    void registerHandlers();

  private:
    using Callback = std::function<void(const drogon::HttpResponsePtr &)>;

    // 서비스가 반환한 게시글 DTO를 프론트엔드가 사용하는 JSON 응답 형태로 변환한다.
    static Json::Value buildPostJson(const BoardPostDTO &post);

    // 브라우저에서 호출되는 API라서 각 응답에 CORS 헤더를 동일하게 붙인다.
    static void applyCors(const drogon::HttpRequestPtr &request,
                          const drogon::HttpResponsePtr &response);

    // 현재 로그인 사용자를 X-Member-Id 헤더에서 읽는다.
    // 값이 없거나 숫자로 파싱할 수 없으면 인증 실패로 처리할 수 있도록 nullopt를 반환한다.
    static std::optional<std::uint64_t> extractMemberIdHeader(
        const drogon::HttpRequestPtr &request);

    // 삭제 요청에서 대상 게시글 ID를 쿼리 파라미터로 읽는다.
    // 잘못된 ID는 서비스까지 보내지 않고 컨트롤러에서 400 응답으로 정리한다.
    static std::optional<std::uint64_t> extractPostIdParameter(
        const drogon::HttpRequestPtr &request);

    // GET /api/board/posts 요청을 처리하며 type 필터가 있으면 서비스로 전달한다.
    void handleListPosts(const drogon::HttpRequestPtr &request, Callback &&callback);

    // POST /api/board/posts 요청을 처리하며 JSON 본문을 CreateBoardPostRequestDTO로 변환한다.
    void handleCreatePost(const drogon::HttpRequestPtr &request, Callback &&callback);

    // DELETE /api/board/posts 요청을 처리하며 본인 게시글인지 검사는 서비스에 맡긴다.
    void handleDeletePost(const drogon::HttpRequestPtr &request, Callback &&callback);

    BoardService boardService_;
};

}  // namespace board
