#pragma once

#include <optional>
#include <string>

#include "../models/MemberTypes.h"

namespace auth
{

// members 테이블에 접근하는 DB 계층이다.
// Service는 SQL을 직접 알지 않고 이 mapper의 함수만 호출한다.
class MemberMapper
{
  public:
    MemberMapper() = default;

    // 로그인과 회원가입 중복 검사에 사용한다.
    std::optional<MemberModel> findByEmail(const std::string &email) const;
    // 세션 토큰에서 얻은 memberId로 현재 회원 정보를 다시 조회할 때 사용한다.
    std::optional<MemberModel> findById(std::uint64_t memberId) const;
    // 회원가입 성공 시 새 members row를 생성한다.
    std::optional<MemberModel> createMember(const std::string &email,
                                            const std::string &passwordHash,
                                            const std::string &name) const;
    // 로그인/회원가입 직후 마지막 로그인 시간을 현재 시각으로 갱신한다.
    void updateLastLoginAt(std::uint64_t memberId) const;
    // 레벨/경험치를 DB에 반영한다.
    void updateLevelAndExp(std::uint64_t memberId,
                           unsigned int level,
                           unsigned int exp) const;
    // password_hash 같은 내부 필드를 제외하고 API 응답용 DTO로 바꾼다.
    MemberDTO toMemberDTO(const MemberModel &member) const;

  private:
    // 로컬 개발 환경에서 members 테이블이 없으면 생성한다.
    void ensureMembersTable() const;
    // 문자열 parameter 하나를 받는 단건 회원 조회 helper다.
    std::optional<MemberModel> querySingleMember(const std::string &sql,
                                                 const std::string &value) const;
    // 숫자 memberId parameter를 받는 단건 회원 조회 helper다.
    std::optional<MemberModel> querySingleMemberById(const std::string &sql,
                                                     std::uint64_t memberId) const;
};

}  // namespace auth
