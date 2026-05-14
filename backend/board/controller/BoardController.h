/*
 * 파일 역할:
 * - 게시판 HTTP 엔드포인트(`/api/board/posts`)를 Drogon 앱에 등록한다.
 * - HTTP 요청 파라미터/헤더/JSON 본문을 서비스 계층 DTO로 변환한다.
 * - 서비스 결과를 API 응답 스키마(JSON + 상태코드 + CORS)로 표준화한다.
 */
#pragma once

#include <drogon/HttpTypes.h>
#include <drogon/drogon.h>

#include <cstdint>
#include <functional>
#include <optional>
#include <string>

#include "board/service/BoardService.h"

namespace board
{

// 게시판 HTTP 엔드포인트를 등록하고 요청/응답 변환 규칙을 담당하는 컨트롤러.
class BoardController
{
  public:
    BoardController() = default;

    // GET/POST/DELETE 라우트를 Drogon 앱에 등록한다.
    // 애플리케이션 부팅 시 1회 호출되는 초기화 진입점이다.
    void registerHandlers();

  private:
    using Callback = std::function<void(const drogon::HttpResponsePtr &)>;

    // 서비스 계층의 게시글 DTO를 API 응답 JSON 스키마로 직렬화한다.
    static Json::Value buildPostJson(const BoardPostDTO &post);
    // 서비스 계층의 페이징 DTO를 API 응답 pagination JSON으로 직렬화한다.
    static Json::Value buildPaginationJson(const BoardPaginationDTO &pagination);
    // 상태 코드와 본문을 공통 규칙(CORS 포함)으로 응답한다.
    // 성공/실패 여부와 무관하게 모든 JSON 응답의 단일 출구 역할을 한다.
    static void sendJson(const drogon::HttpRequestPtr &request,
                         const Callback &callback,
                         const Json::Value &body,
                         drogon::HttpStatusCode statusCode);
    // 에러 응답을 공통 규칙(CORS 포함)으로 응답한다.
    // 내부적으로 makeErrorBody 형태의 최소 스키마를 생성해 sendJson으로 전달한다.
    static void sendError(const drogon::HttpRequestPtr &request,
                          const Callback &callback,
                          drogon::HttpStatusCode statusCode,
                          const std::string &message);

    // 요청 Origin을 기준으로 CORS 헤더를 응답에 채운다.
    // credential 포함 요청을 허용하기 위해 Allow-Credentials/Headers/Methods를 함께 설정한다.
    static void applyCors(const drogon::HttpRequestPtr &request,
                          const drogon::HttpResponsePtr &response);

    // 인증 헤더(X-Member-Id)에서 회원 ID를 읽어 숫자로 파싱한다.
    // 값이 없거나 숫자 변환 실패, 0 이하인 경우 nullopt를 반환한다.
    static std::optional<std::uint64_t> extractMemberIdHeader(
        const drogon::HttpRequestPtr &request);

    // 쿼리 파라미터(postId)에서 게시글 ID를 읽어 숫자로 파싱한다.
    // 값이 없거나 숫자 변환 실패, 0 이하인 경우 nullopt를 반환한다.
    static std::optional<std::uint64_t> extractPostIdParameter(
        const drogon::HttpRequestPtr &request);

    // 문자열 파라미터를 양의 정수로 파싱하고 min/max 범위를 함께 검증한다.
    // from_chars를 사용해 예외 없이 파싱하며, 전체 문자열이 숫자인지까지 확인한다.
    static std::optional<std::uint32_t> parsePositiveIntegerParameter(
        const std::string &value,
        std::uint32_t minValue,
        std::uint32_t maxValue);

    // 게시글 목록 조회 요청을 처리한다.
    // 쿼리 파라미터 검증 -> 서비스 호출 -> JSON 응답 변환 순서로 동작한다.
    void handleListPosts(const drogon::HttpRequestPtr &request, Callback &&callback);
    // 게시글 생성 요청을 처리한다.
    // 인증 헤더 확인 -> JSON 본문 검증 -> 서비스 호출 -> 생성 결과 응답 순으로 동작한다.
    void handleCreatePost(const drogon::HttpRequestPtr &request, Callback &&callback);
    // 게시글 삭제 요청을 처리한다.
    // 인증 헤더와 postId를 검증한 뒤 서비스 계층의 권한 검증/삭제 로직을 호출한다.
    void handleDeletePost(const drogon::HttpRequestPtr &request, Callback &&callback);

    // 게시판 비즈니스 로직 진입점.
    // 컨트롤러는 DB 접근 없이 서비스 계층으로만 의존한다.
    BoardService boardService_;
};

}  // namespace board
