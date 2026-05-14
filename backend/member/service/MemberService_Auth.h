/*
 * 파일 개요: 인증 도메인(Auth) 하위 서비스 인터페이스다.
 * 주요 역할: 회원가입/로그인/비밀번호 변경 API 계약과 인증 보조 유틸 선언.
 */
#pragma once

#include <optional>
#include <string>

#include "member/mapper/MemberMapper.h"
#include "member/model/MemberTypes.h"
#include "member/service/MemberService_SessionStore.h"

namespace auth
{

// 회원가입/로그인/비밀번호 변경 등 인증 도메인을 담당한다.
class MemberService_Auth
{
  public:
    MemberService_Auth(MemberMapper &mapper, MemberService_SessionStore &sessionStore)
        : mapper_(mapper), sessionStore_(sessionStore)
    {
    }

    // 회원가입 유스케이스를 수행한다.
    // 성공 시 생성된 회원 정보와 새 세션 토큰을 반환한다.
    AuthResultDTO signup(const SignupRequestDTO &request);

    // 로그인 유스케이스를 수행한다.
    // 계정 존재 여부를 직접 노출하지 않는 공통 오류 정책을 따른다.
    AuthResultDTO login(const LoginRequestDTO &request);

    // 인증된 사용자 비밀번호를 변경한다.
    // 세션 검증 + 현재 비밀번호 검증을 통과한 경우에만 갱신한다.
    ChangePasswordResultDTO changePassword(
        const std::string &sessionToken,
        const ChangePasswordRequestDTO &request);

  private:
    // 문자열 정규화/검증 유틸.
    // trim/toLower는 이메일 비교와 빈값 검증 시 일관성을 맞춘다.
    static std::string trim(std::string value);
    static std::string toLower(std::string value);
    static bool isValidEmail(const std::string &email);

    // 비밀번호 해시/검증 유틸.
    // 저장 포맷은 "salt$hash"를 사용하며 verifyPassword가 이를 검증한다.
    static std::string randomHex(std::size_t length);
    static std::string hashPasswordWithSalt(const std::string &password);
    static bool verifyPassword(const std::string &password,
                               const std::string &storedHash);

    // mapper_: 회원 조회/갱신 담당, sessionStore_: 로그인 세션 발급/검증 담당
    MemberMapper &mapper_;
    MemberService_SessionStore &sessionStore_;
};

}  // namespace auth

