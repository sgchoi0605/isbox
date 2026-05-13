#pragma once

// Drogon HTTP 타입 및 앱 프레임워크 사용을 위한 헤더
#include <drogon/HttpTypes.h>
#include <drogon/drogon.h>

// 고정 폭 정수, 콜백, 선택적 값, 문자열 타입 사용
#include <cstdint>
#include <functional>
#include <optional>
#include <string>

// 친구 도메인 DTO/서비스 및 인증용 회원 서비스 의존성
#include "../models/FriendTypes.h"
#include "../services/FriendService.h"
#include "../services/MemberService.h"

namespace friendship
{

// 친구 관련 HTTP 엔드포인트를 등록하고 요청을 처리하는 컨트롤러
class FriendController
{
  public:
    // 현재 로그인 사용자 판별을 위해 MemberService 참조를 주입받는다.
    explicit FriendController(auth::MemberService &memberService)
        : memberService_(memberService)
    {
    }

    // 친구 API 라우트(엔드포인트)를 Drogon 앱에 등록한다.
    void registerHandlers();

  private:
    // Drogon 핸들러 응답 콜백 타입 별칭
    using Callback = std::function<void(const drogon::HttpResponsePtr &)>;

    // 친구 검색 결과의 회원 DTO를 JSON으로 변환한다.
    static Json::Value buildMemberJson(const FriendMemberDTO &member);
    // 친구 목록 DTO를 JSON으로 변환한다.
    static Json::Value buildFriendJson(const FriendDTO &friendItem);
    // 친구 요청 DTO를 JSON으로 변환한다.
    static Json::Value buildRequestJson(const FriendRequestDTO &request);
    // 응답에 CORS 헤더를 설정한다.
    static void applyCors(const drogon::HttpRequestPtr &request,
                          const drogon::HttpResponsePtr &response);
    // 요청 쿠키에서 세션 토큰 값을 추출한다.
    static std::string extractSessionToken(const drogon::HttpRequestPtr &request);
    // URL 경로의 friendshipId 문자열을 양의 정수 ID로 파싱한다.
    static std::optional<std::uint64_t> parseFriendshipId(
        const std::string &friendshipIdValue);

    // 세션 토큰을 기준으로 현재 로그인한 회원 ID를 구한다.
    std::optional<std::uint64_t> extractCurrentMemberId(
        const drogon::HttpRequestPtr &request);

    // 이메일로 친구 검색 요청을 처리한다.
    void handleSearchMember(const drogon::HttpRequestPtr &request, Callback &&callback);
    // 친구 요청 전송을 처리한다.
    void handleSendRequest(const drogon::HttpRequestPtr &request, Callback &&callback);
    // 수락된 친구 목록 조회를 처리한다.
    void handleListFriends(const drogon::HttpRequestPtr &request, Callback &&callback);
    // 받은 친구 요청 목록 조회를 처리한다.
    void handleListIncomingRequests(const drogon::HttpRequestPtr &request,
                                    Callback &&callback);
    // 보낸 친구 요청 목록 조회를 처리한다.
    void handleListOutgoingRequests(const drogon::HttpRequestPtr &request,
                                    Callback &&callback);
    // 특정 친구 요청 수락을 처리한다.
    void handleAcceptRequest(const drogon::HttpRequestPtr &request,
                             Callback &&callback,
                             const std::string &friendshipIdValue);
    // 특정 친구 요청 거절을 처리한다.
    void handleRejectRequest(const drogon::HttpRequestPtr &request,
                             Callback &&callback,
                             const std::string &friendshipIdValue);
    // 특정 친구 요청 취소를 처리한다.
    void handleCancelRequest(const drogon::HttpRequestPtr &request,
                             Callback &&callback,
                             const std::string &friendshipIdValue);

    // 로그인/세션 판별에 사용하는 회원 서비스 참조
    auth::MemberService &memberService_;
    // 친구 도메인 로직(검색/요청/수락/거절/취소)을 수행하는 서비스
    FriendService friendService_;
};

}  // namespace friendship

