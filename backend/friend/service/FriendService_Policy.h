/*
 * 파일 역할 요약:
 * - 친구 서비스 전반에서 공통으로 쓰는 입력 정규화/검증 규칙을 선언한다.
 * - 이메일 형식 검증과 회원 활성 상태 판정 규칙을 한곳에 모아 재사용한다.
 */
#pragma once

#include <string>

namespace friendship
{

// 친구 서비스에서 공통으로 쓰는 문자열/검증 규칙 유틸리티.
// 입력값 정규화/검증 기준을 한곳에 모아 Query/Command 간 동작을 일치시킨다.
class FriendService_Policy
{
  public:
    // 문자열 앞뒤 공백을 제거한다.
    static std::string trim(std::string value);
    // 문자열을 소문자로 정규화한다.
    static std::string toLower(std::string value);
    // 최소한의 이메일 형식을 검사한다.
    static bool isValidEmail(const std::string &email);
    // 회원 상태가 활성 상태인지 확인한다.
    static bool isActiveMemberStatus(const std::string &status);
};

}  // namespace friendship
