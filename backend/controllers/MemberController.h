#pragma once

// HTTP 상태코드/메서드 열거형을 사용하기 위해 포함한다.
#include <drogon/HttpTypes.h>
// Drogon 요청/응답 타입과 라우팅 등록 API를 사용하기 위해 포함한다.
#include <drogon/drogon.h>

// 콜백 타입(std::function)을 선언하기 위해 포함한다.
#include <functional>
// 선택적 값(std::optional)을 사용하기 위해 포함한다.
#include <optional>
// 문자열(std::string) 처리를 위해 포함한다.
#include <string>

// 회원/인증 비즈니스 로직을 처리하는 서비스 계층을 포함한다.
#include "../services/MemberService.h"

namespace auth
{

// 로그인, 회원가입, 현재 사용자 조회, 로그아웃 HTTP API를 담당한다.
class MemberController
{
  public:
    explicit MemberController(MemberService &service) : service_(service) {}

    // main.cpp에서 호출되어 인증 관련 URL과 handler를 Drogon에 등록한다.
    void registerHandlers();

  private:
    // Drogon 핸들러의 완료 콜백 타입을 별칭으로 정의한다.
    using Callback = std::function<void(const drogon::HttpResponsePtr &)>;

    // 서비스 계층의 MemberDTO를 프론트엔드가 쓰는 JSON 응답 형태로 바꾼다.
    static Json::Value buildMemberJson(const MemberDTO &member);
    // Food MBTI DTO를 JSON 응답 객체로 변환한다.
    static Json::Value buildFoodMbtiJson(const FoodMbtiDTO &foodMbti);
    // 회원 프로필 DTO를 JSON 응답 객체로 변환한다.
    static Json::Value buildProfileJson(const MemberProfileDTO &profile);
    // 브라우저에서 API를 호출할 수 있도록 인증 API 응답에 CORS 헤더를 붙인다.
    static void applyCors(const drogon::HttpRequestPtr &request,
                          const drogon::HttpResponsePtr &response);
    // 현재 로그인 세션을 확인하기 위해 요청 쿠키에서 세션 토큰을 꺼낸다.
    static std::string extractSessionToken(const drogon::HttpRequestPtr &request);
    // URL path의 문자열 memberId를 양의 정수 ID로 파싱한다.
    // 값이 비어 있거나 숫자가 아니거나 0이면 std::nullopt를 반환한다.
    static std::optional<std::uint64_t> parseMemberId(
        const std::string &memberIdValue);
    // 로그인/회원가입 성공 또는 로그아웃 시 브라우저에 내려줄 세션 쿠키 문자열을 만든다.
    static std::string buildSessionCookie(const std::string &token,
                                          int maxAgeSeconds);

    // POST /api/auth/signup 요청을 처리한다.
    void handleSignup(const drogon::HttpRequestPtr &request, Callback &&callback);
    // POST /api/auth/login 요청을 처리한다.
    void handleLogin(const drogon::HttpRequestPtr &request, Callback &&callback);
    // GET /api/members/me 요청으로 현재 로그인 사용자를 확인한다.
    void handleMe(const drogon::HttpRequestPtr &request, Callback &&callback);
    // GET /api/members/profile 요청으로 현재 로그인 사용자의 프로필을 조회한다.
    void handleGetMyProfile(const drogon::HttpRequestPtr &request,
                            Callback &&callback);
    // GET /api/members/{memberId}/profile 요청으로 특정 회원 프로필을 조회한다.
    void handleGetMemberProfile(const drogon::HttpRequestPtr &request,
                                Callback &&callback,
                                const std::string &memberIdValue);
    // PUT /api/members/profile 요청으로 현재 로그인 사용자의 기본 정보를 수정한다.
    void handleUpdateProfile(const drogon::HttpRequestPtr &request,
                             Callback &&callback);
    // PUT /api/members/password 요청으로 현재 로그인 사용자의 비밀번호를 변경한다.
    void handleChangePassword(const drogon::HttpRequestPtr &request,
                              Callback &&callback);
    // POST /api/members/exp 요청으로 현재 로그인 사용자 경험치를 지급한다.
    void handleAwardExperience(const drogon::HttpRequestPtr &request,
                               Callback &&callback);
    // PUT /api/members/me/food-mbti 요청으로 Food MBTI 결과를 저장한다.
    void handleSaveFoodMbti(const drogon::HttpRequestPtr &request,
                            Callback &&callback);
    // POST /api/auth/logout 요청으로 현재 세션을 제거한다.
    void handleLogout(const drogon::HttpRequestPtr &request, Callback &&callback);

    // 입력 검증, 비밀번호 확인, 세션 생성 같은 인증 비즈니스 로직은 서비스에 위임한다.
    MemberService &service_;
};

}  // namespace auth
