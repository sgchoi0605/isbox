#pragma once

#include <drogon/HttpTypes.h>
#include <drogon/drogon.h>

#include <cstdint>
#include <functional>
#include <optional>
#include <string>

#include "../services/BoardService.h"

namespace board
{

// 게시판 API 요청을 받아 서비스 계층으로 전달하는 컨트롤러
class BoardController
{
  public:
    BoardController() = default;

    // GET/POST/DELETE 게시글 API 핸들러를 등록한다.
    void registerHandlers();

  private:
    // Drogon 응답 콜백 타입 별칭
    using Callback = std::function<void(const drogon::HttpResponsePtr &)>;

    // 서비스에서 받은 게시글 DTO를 API 응답용 JSON으로 변환한다.
    static Json::Value buildPostJson(const BoardPostDTO &post);

    // 모든 게시판 응답에 공통 CORS 헤더를 추가한다.
    static void applyCors(const drogon::HttpRequestPtr &request,
                          const drogon::HttpResponsePtr &response);

    // 인증 헤더(X-Member-Id)에서 로그인 사용자 ID를 추출한다.
    // 값이 없거나 숫자가 아니면 nullopt를 반환한다.
    static std::optional<std::uint64_t> extractMemberIdHeader(
        const drogon::HttpRequestPtr &request);

    // 쿼리 파라미터 postId를 추출한다.
    // 값이 없거나 숫자가 아니면 nullopt를 반환한다.
    static std::optional<std::uint64_t> extractPostIdParameter(
        const drogon::HttpRequestPtr &request);

    // GET /api/board/posts 요청을 처리한다.
    void handleListPosts(const drogon::HttpRequestPtr &request, Callback &&callback);

    // POST /api/board/posts 요청을 처리한다.
    void handleCreatePost(const drogon::HttpRequestPtr &request, Callback &&callback);

    // DELETE /api/board/posts 요청을 처리한다.
    void handleDeletePost(const drogon::HttpRequestPtr &request, Callback &&callback);

    // 게시판 비즈니스 로직을 담당하는 서비스 객체
    BoardService boardService_;
};

}  // namespace board
