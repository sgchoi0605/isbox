/*
 * 파일 개요: 회원 인증/프로필 관련 API 엔드포인트를 노출하는 컨트롤러 인터페이스다.
 * 주요 역할: 핸들러 시그니처 정의, 공통 파싱/응답 유틸 선언, 서비스 의존성 연결.
 */
#pragma once

// Drogon HTTP 타입과 상태 코드를 사용한다.
#include <drogon/HttpTypes.h>
// 라우터 등록, 요청/응답 객체를 사용한다.
#include <drogon/drogon.h>

// 비동기 응답 콜백 타입(std::function)을 사용한다.
#include <functional>
// 파싱 결과의 성공/실패 표현(std::optional)을 사용한다.
#include <optional>
// 문자열 파라미터를 처리한다.
#include <string>

// 비즈니스 로직은 서비스 계층으로 위임한다.
#include "member/service/MemberService.h"

namespace auth
{

// 회원 인증/프로필 관련 HTTP 엔드포인트를 담당한다.
// 컨트롤러는 "요청 파싱/응답 직렬화"까지만 수행하고,
// 실제 도메인 규칙(검증, DB 반영, 권한 판별)은 MemberService로 위임한다.
class MemberController
{
  public:
    explicit MemberController(MemberService &service) : service_(service) {}

    // API 경로와 핸들러를 Drogon 앱에 등록한다.
    // 서버 시작 시 1회 호출되며, 이후 요청마다 등록된 핸들러가 실행된다.
    void registerHandlers();

  private:
    // 핸들러에서 공통으로 사용하는 응답 콜백 시그니처다.
    using Callback = std::function<void(const drogon::HttpResponsePtr &)>;

    // DTO를 API 응답 JSON 형식으로 변환한다.
    // 컨트롤러 외부(서비스/매퍼)에서는 JSON 직렬화 포맷을 알 필요가 없다.
    static Json::Value buildMemberJson(const MemberDTO &member);
    static Json::Value buildFoodMbtiJson(const FoodMbtiDTO &foodMbti);
    static Json::Value buildProfileJson(const MemberProfileDTO &profile);

    // 쿠키 기반 인증 요청을 위해 CORS 헤더를 공통 적용한다.
    // 브라우저의 자격증명 요청(쿠키 포함) 시 필수 헤더를 빠뜨리지 않기 위한 공통 지점이다.
    static void applyCors(const drogon::HttpRequestPtr &request,
                          const drogon::HttpResponsePtr &response);
    // 요청 쿠키에서 세션 토큰을 추출한다.
    // 토큰이 없거나 쿠키가 비어 있으면 빈 문자열을 반환한다.
    static std::string extractSessionToken(const drogon::HttpRequestPtr &request);
    // 경로 파라미터 memberId를 안전하게 정수로 파싱한다.
    // 숫자 형식이 아니거나 범위를 벗어나면 nullopt를 반환한다.
    static std::optional<std::uint64_t> parseMemberId(
        const std::string &memberIdValue);
    // Set-Cookie 헤더 값을 생성한다.
    // HttpOnly/SameSite/Path/Max-Age 정책을 한 곳에서 일관되게 조합한다.
    static std::string buildSessionCookie(const std::string &token,
                                          int maxAgeSeconds);

    // 각 API 엔드포인트의 실제 처리 함수다.
    // 공통 흐름:
    // 1) 요청 파라미터/JSON 파싱
    // 2) 서비스 호출
    // 3) HTTP 상태코드 + JSON 응답 구성
    void handleSignup(const drogon::HttpRequestPtr &request, Callback &&callback);
    void handleLogin(const drogon::HttpRequestPtr &request, Callback &&callback);
    void handleMe(const drogon::HttpRequestPtr &request, Callback &&callback);
    void handleGetMyProfile(const drogon::HttpRequestPtr &request,
                            Callback &&callback);
    void handleGetMemberProfile(const drogon::HttpRequestPtr &request,
                                Callback &&callback,
                                const std::string &memberIdValue);
    void handleUpdateProfile(const drogon::HttpRequestPtr &request,
                             Callback &&callback);
    void handleChangePassword(const drogon::HttpRequestPtr &request,
                              Callback &&callback);
    void handleAwardExperience(const drogon::HttpRequestPtr &request,
                               Callback &&callback);
    void handleSaveFoodMbti(const drogon::HttpRequestPtr &request,
                            Callback &&callback);
    void handleLogout(const drogon::HttpRequestPtr &request, Callback &&callback);

    // 검증/인증/DB 업데이트 등 비즈니스 규칙은 서비스로 위임한다.
    MemberService &service_;
};

}  // namespace auth

