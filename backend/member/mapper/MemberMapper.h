/*
 * 파일 개요: 회원 도메인 DB 접근을 캡슐화한 데이터 매퍼 인터페이스다.
 * 주요 역할: 서비스 계층에 SQL 세부사항을 숨기고 필요한 CRUD 메서드 계약을 제공.
 */
#pragma once

#include <optional>
#include <string>

#include "member/model/MemberTypes.h"

namespace auth
{

// members/member_food_mbti 테이블 접근을 담당하는 데이터 매퍼다.
// 서비스 계층은 SQL 문자열/스키마 세부사항을 직접 다루지 않고
// 이 인터페이스만 호출해 데이터 입출력을 수행한다.
class MemberMapper
{
  public:
    MemberMapper() = default;

    // 고유 이메일 기준 단일 회원을 조회한다.
    // 조회 결과가 없으면 nullopt를 반환한다.
    std::optional<MemberModel> findByEmail(const std::string &email) const;

    // 회원 ID 기준 단일 회원을 조회한다.
    // soft-delete 개념이 없다면 존재하지 않는 ID에서 nullopt가 반환된다.
    std::optional<MemberModel> findById(std::uint64_t memberId) const;

    // 새 회원 row를 생성하고 생성된 회원 정보를 반환한다.
    // 실패하거나 생성 직후 재조회에 실패하면 nullopt를 반환할 수 있다.
    std::optional<MemberModel> createMember(const std::string &email,
                                            const std::string &passwordHash,
                                            const std::string &name) const;

    // 마지막 로그인 시각을 현재 시각으로 갱신한다.
    // 인증 성공 직후 호출되어 감사/접속기록성 메타데이터를 유지한다.
    void updateLastLoginAt(std::uint64_t memberId) const;

    // 이메일/이름 프로필 정보를 갱신한다.
    // 중복 이메일 제약 위반은 호출측에서 예외 처리로 분기한다.
    void updateProfile(std::uint64_t memberId,
                       const std::string &email,
                       const std::string &name) const;

    // 비밀번호 해시를 갱신한다.
    // 평문 비밀번호는 절대 저장하지 않으며, 호출자가 해시값을 전달한다.
    void updatePasswordHash(std::uint64_t memberId,
                            const std::string &passwordHash) const;

    // 레벨/경험치 값을 갱신한다.
    // 경험치 지급 규칙 계산은 서비스가 담당하고, 매퍼는 반영만 담당한다.
    void updateLevelAndExp(std::uint64_t memberId,
                           unsigned int level,
                           unsigned int exp) const;

    // Food MBTI를 삽입 또는 갱신(UPSERT)한다.
    // member_id를 유니크 키로 사용해 "없으면 INSERT, 있으면 UPDATE"를 수행한다.
    void upsertFoodMbti(std::uint64_t memberId,
                        const std::string &type,
                        const std::string &title,
                        const std::string &description,
                        const std::string &traitsJson,
                        const std::string &recommendedFoodsJson,
                        const std::optional<std::string> &completedAt) const;

    // 회원의 Food MBTI를 조회한다.
    // 결과가 없는 경우 nullopt를 반환해 "아직 검사 미완료" 상태를 표현한다.
    std::optional<FoodMbtiModel> findFoodMbtiByMemberId(
        std::uint64_t memberId) const;

    // DB 모델을 API 응답용 DTO로 변환한다(민감정보 제외).
    // passwordHash 같은 민감 필드를 외부 계층으로 노출하지 않는다.
    MemberDTO toMemberDTO(const MemberModel &member) const;

  private:
    // 로컬 개발 환경에서 필요한 기본 테이블을 보장한다.
    // 앱 시작 직후 첫 쿼리 시점에 한 번만 수행되도록 구현된다.
    void ensureMembersTable() const;
    void ensureMemberFoodMbtiTable() const;

    // 문자열 파라미터 바인딩 기반 단일 회원 조회 공통 함수다.
    // Prepared statement 바인딩을 사용해 SQL 인젝션 위험을 줄인다.
    std::optional<MemberModel> querySingleMember(const std::string &sql,
                                                 const std::string &value) const;

    // 숫자 memberId 파라미터 바인딩 기반 단일 회원 조회 공통 함수다.
    // member_id 기반 조회 로직 중복을 줄이기 위한 헬퍼다.
    std::optional<MemberModel> querySingleMemberById(const std::string &sql,
                                                     std::uint64_t memberId) const;
};

}  // namespace auth

