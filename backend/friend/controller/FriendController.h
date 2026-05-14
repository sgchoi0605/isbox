/*
 * 파일 역할 요약:
 * - 친구 기능 HTTP 컨트롤러의 공개 인터페이스를 선언한다.
 * - 엔드포인트별 처리 함수, 인증 추출 함수, JSON 직렬화 함수를 정의한다.
 */
#pragma once

#include <drogon/HttpTypes.h>
#include <drogon/drogon.h>

#include <cstdint>
#include <functional>
#include <optional>
#include <string>

#include "friend/model/FriendTypes.h"
#include "friend/service/FriendService.h"
#include "member/service/MemberService.h"

namespace friendship
{

// 친구 기능 HTTP 엔드포인트를 등록하고 요청을 처리하는 컨트롤러.
// - 인증 쿠키에서 로그인 회원을 식별한다.
// - 요청 파라미터/본문을 검증한 뒤 FriendService로 위임한다.
// - 서비스 결과를 HTTP 상태코드 + JSON 응답으로 직렬화한다.
class FriendController
{
  public:
    // 세션 토큰 기반 사용자 식별을 위해 MemberService를 주입받는다.
    explicit FriendController(auth::MemberService &memberService)
        : memberService_(memberService)
    {
    }

    // Drogon 앱에 친구 관련 라우트를 등록한다.
    void registerHandlers();

  private:
    // Drogon 핸들러 응답 콜백 타입 별칭.
    // 모든 핸들러는 이 콜백을 정확히 한 번 호출해 응답을 완료해야 한다.
    using Callback = std::function<void(const drogon::HttpResponsePtr &)>;

    // DTO -> JSON 직렬화 유틸리티.
    static Json::Value buildMemberJson(const FriendMemberDTO &member);
    static Json::Value buildFriendJson(const FriendDTO &friendItem);
    static Json::Value buildRequestJson(const FriendRequestDTO &request);

    // CORS 응답 헤더를 설정한다.
    static void applyCors(const drogon::HttpRequestPtr &request,
                          const drogon::HttpResponsePtr &response);

    // 요청 쿠키에서 세션 토큰을 꺼낸다.
    static std::string extractSessionToken(const drogon::HttpRequestPtr &request);
    // 경로 파라미터 friendshipId 문자열을 양의 정수로 파싱한다.
    static std::optional<std::uint64_t> parseFriendshipId(
        const std::string &friendshipIdValue);

    // 세션 토큰으로 현재 로그인 회원 ID를 조회한다.
    std::optional<std::uint64_t> extractCurrentMemberId(
        const drogon::HttpRequestPtr &request);

    // 각 API 엔드포인트별 요청 처리기.
    // 구현 공통 흐름: 인증 확인 -> 입력 검증 -> 서비스 호출 -> 응답 구성.
    void handleSearchMember(const drogon::HttpRequestPtr &request, Callback &&callback);
    void handleSendRequest(const drogon::HttpRequestPtr &request, Callback &&callback);
    void handleListFriends(const drogon::HttpRequestPtr &request, Callback &&callback);
    void handleListIncomingRequests(const drogon::HttpRequestPtr &request,
                                    Callback &&callback);
    void handleListOutgoingRequests(const drogon::HttpRequestPtr &request,
                                    Callback &&callback);
    void handleAcceptRequest(const drogon::HttpRequestPtr &request,
                             Callback &&callback,
                             const std::string &friendshipIdValue);
    void handleRejectRequest(const drogon::HttpRequestPtr &request,
                             Callback &&callback,
                             const std::string &friendshipIdValue);
    void handleCancelRequest(const drogon::HttpRequestPtr &request,
                             Callback &&callback,
                             const std::string &friendshipIdValue);
    void handleRemoveFriend(const drogon::HttpRequestPtr &request,
                            Callback &&callback,
                            const std::string &friendshipIdValue);

    // 인증 사용자 식별용 서비스.
    auth::MemberService &memberService_;
    // 친구 도메인 유스케이스 서비스.
    FriendService friendService_;
};

}  // namespace friendship

