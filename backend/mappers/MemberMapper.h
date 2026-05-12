#pragma once

#include <optional>
#include <string>

#include "../models/MemberTypes.h"

namespace auth
{

// members 테이블을 대상으로 동작하는 데이터 접근 계층이다.
// 서비스 레이어는 SQL을 직접 다루지 않고, 이 Mapper의 함수만 호출한다.
class MemberMapper
{
  public:
    // 별도 상태가 없는 유틸리티성 Mapper이므로 기본 생성자를 사용한다.
    MemberMapper() = default;

    // 이메일로 회원 1건을 조회한다.
    // 로그인 처리, 회원가입 중복 체크에 사용한다.
    std::optional<MemberModel> findByEmail(const std::string &email) const;

    // memberId로 회원 1건을 조회한다.
    // 세션 토큰 검증 후 최신 회원 정보를 다시 읽어올 때 사용한다.
    std::optional<MemberModel> findById(std::uint64_t memberId) const;

    // 신규 회원 row를 생성한다.
    // passwordHash는 서비스에서 생성해 전달하며, Mapper는 저장만 담당한다.
    std::optional<MemberModel> createMember(const std::string &email,
                                            const std::string &passwordHash,
                                            const std::string &name) const;

    // 마지막 로그인 시각을 현재 시각으로 갱신한다.
    // 로그인 성공 직후 호출된다.
    void updateLastLoginAt(std::uint64_t memberId) const;

    void updateProfile(std::uint64_t memberId,
                       const std::string &email,
                       const std::string &name) const;

    void updatePasswordHash(std::uint64_t memberId,
                            const std::string &passwordHash) const;

    // 레벨/경험치를 DB에 반영한다.
    // 게임화 요소나 활동 보상 반영 시 사용한다.
    void updateLevelAndExp(std::uint64_t memberId,
                           unsigned int level,
                           unsigned int exp) const;

    void upsertFoodMbti(std::uint64_t memberId,
                        const std::string &type,
                        const std::string &title,
                        const std::string &description,
                        const std::string &traitsJson,
                        const std::string &recommendedFoodsJson,
                        const std::optional<std::string> &completedAt) const;

    std::optional<FoodMbtiModel> findFoodMbtiByMemberId(
        std::uint64_t memberId) const;

    // 내부 모델(MemberModel)을 외부 응답용 DTO(MemberDTO)로 변환한다.
    // 민감 정보(passwordHash)는 의도적으로 제외한다.
    MemberDTO toMemberDTO(const MemberModel &member) const;

  private:
    // 로컬 개발 환경에서 members 테이블이 없으면 최소 스키마를 생성한다.
    void ensureMembersTable() const;
    void ensureMemberFoodMbtiTable() const;

    // 문자열 파라미터 1개를 바인딩해 단일 회원을 조회하는 공통 헬퍼다.
    std::optional<MemberModel> querySingleMember(const std::string &sql,
                                                 const std::string &value) const;

    // 숫자 memberId 파라미터를 바인딩해 단일 회원을 조회하는 공통 헬퍼다.
    std::optional<MemberModel> querySingleMemberById(const std::string &sql,
                                                     std::uint64_t memberId) const;
};

}  // namespace auth
