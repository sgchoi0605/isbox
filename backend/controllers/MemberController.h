#pragma once

#include <drogon/HttpTypes.h>
#include <drogon/drogon.h>

#include <functional>
#include <string>

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
    using Callback = std::function<void(const drogon::HttpResponsePtr &)>;

    // 서비스 계층의 MemberDTO를 프론트엔드가 쓰는 JSON 응답 형태로 바꾼다.
    static Json::Value buildMemberJson(const MemberDTO &member);
    // 브라우저에서 API를 호출할 수 있도록 인증 API 응답에 CORS 헤더를 붙인다.
    static void applyCors(const drogon::HttpRequestPtr &request,
                          const drogon::HttpResponsePtr &response);
    // 현재 로그인 세션을 확인하기 위해 요청 쿠키에서 세션 토큰을 꺼낸다.
    static std::string extractSessionToken(const drogon::HttpRequestPtr &request);
    // 로그인/회원가입 성공 또는 로그아웃 시 브라우저에 내려줄 세션 쿠키 문자열을 만든다.
    static std::string buildSessionCookie(const std::string &token,
                                          int maxAgeSeconds);

    // POST /api/auth/signup 요청을 처리한다.
    void handleSignup(const drogon::HttpRequestPtr &request, Callback &&callback);
    // POST /api/auth/login 요청을 처리한다.
    void handleLogin(const drogon::HttpRequestPtr &request, Callback &&callback);
    // GET /api/members/me 요청으로 현재 로그인 사용자를 확인한다.
    void handleMe(const drogon::HttpRequestPtr &request, Callback &&callback);
    // PUT /api/members/profile 요청으로 현재 로그인 사용자의 기본 정보를 수정한다.
    void handleUpdateProfile(const drogon::HttpRequestPtr &request,
                             Callback &&callback);
    // PUT /api/members/password 요청으로 현재 로그인 사용자의 비밀번호를 변경한다.
    void handleChangePassword(const drogon::HttpRequestPtr &request,
                              Callback &&callback);
    // POST /api/members/exp 요청으로 현재 로그인 사용자 경험치를 지급한다.
    void handleAwardExperience(const drogon::HttpRequestPtr &request,
                               Callback &&callback);
    // POST /api/auth/logout 요청으로 현재 세션을 제거한다.
    void handleLogout(const drogon::HttpRequestPtr &request, Callback &&callback);

    // 입력 검증, 비밀번호 확인, 세션 생성 같은 인증 비즈니스 로직은 서비스에 위임한다.
    MemberService &service_;
};

}  // namespace auth
