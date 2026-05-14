/*
 * 파일 개요: 회원 도메인 통합 진입점(MemberService) 인터페이스다.
 * 주요 역할: 인증/프로필/경험치/비밀번호 변경 유스케이스를 단일 서비스로 묶어 제공.
 */
#pragma once

#include <cstdint>
#include <optional>
#include <string>

#include "friend/mapper/FriendMapper.h"
#include "member/mapper/MemberMapper.h"
#include "member/model/MemberTypes.h"
#include "member/service/MemberService_Auth.h"
#include "member/service/MemberService_Experience.h"
#include "member/service/MemberService_Profile.h"
#include "member/service/MemberService_SessionStore.h"

namespace auth
{

// 컨트롤러가 사용하는 회원 도메인의 통합 서비스 파사드다.
// 실제 세부 로직은 하위 서비스(Auth/Profile/Experience)로 분리한다.
class MemberService
{
  public:
    MemberService();

    // 회원가입을 수행한다.
    // 입력 검증/중복검사/세션발급까지 포함된 결과 DTO를 반환한다.
    AuthResultDTO signup(const SignupRequestDTO &request);

    // 로그인을 수행한다.
    // 성공 시 세션 토큰이 함께 내려가며 실패 원인은 message에 담긴다.
    AuthResultDTO login(const LoginRequestDTO &request);

    // 현재 세션을 만료(로그아웃)한다.
    // 세션 삭제 성공 여부만 bool로 반환한다.
    bool logout(const std::string &sessionToken);

    // 세션 토큰 기준 현재 사용자의 기본 정보를 조회한다.
    // 유효하지 않은 토큰이면 nullopt를 반환한다.
    std::optional<MemberDTO> getCurrentMember(const std::string &sessionToken);

    // 세션 토큰 기준 현재 사용자 ID만 빠르게 조회한다.
    // 인증이 필요하지만 전체 회원 정보가 불필요한 경로에서 사용한다.
    std::optional<std::uint64_t> getCurrentMemberId(const std::string &sessionToken);

    // 이름/이메일 프로필 정보를 수정한다.
    // 중복 이메일 등 비즈니스 오류는 결과 DTO의 statusCode로 표현된다.
    UpdateProfileResultDTO updateProfile(const std::string &sessionToken,
                                         const UpdateProfileRequestDTO &request);

    // 현재 사용자의 Food MBTI 결과를 저장(업서트)한다.
    // 성공 시 저장된 MBTI 데이터를 다시 반환한다.
    SaveFoodMbtiResultDTO saveMyFoodMbti(
        const std::string &sessionToken,
        const SaveFoodMbtiRequestDTO &request);

    // 현재 로그인 사용자의 프로필 상세를 조회한다.
    MemberProfileResultDTO getMyProfile(const std::string &sessionToken);

    // 특정 회원의 공개 가능한 프로필 상세를 조회한다.
    // 친구 관계/본인 여부 판단이 포함된 응답이 반환된다.
    MemberProfileResultDTO getMemberProfile(const std::string &sessionToken,
                                            std::uint64_t targetMemberId);

    // 사용자 행동에 따른 경험치를 지급하고 레벨/경험치를 갱신한다.
    AwardExperienceResultDTO awardExperience(
        const std::string &sessionToken,
        const AwardExperienceRequestDTO &request);

    // 현재 비밀번호 검증 후 새 비밀번호로 변경한다.
    // 보안상 성공 시에도 최소한의 결과 정보만 반환한다.
    ChangePasswordResultDTO changePassword(
        const std::string &sessionToken,
        const ChangePasswordRequestDTO &request);

  private:
    // 공용 의존 객체들
    // - mapper_: 회원 DB 접근
    // - friendMapper_: 친구 관계 조회
    // - sessionStore_: 인증 세션 저장소
    MemberMapper mapper_;
    friendship::FriendMapper friendMapper_;
    MemberService_SessionStore sessionStore_;

    // 도메인별 서브 서비스
    // 컨트롤러는 이 구성을 몰라도 되며 MemberService만 의존하면 된다.
    MemberService_Auth authService_;
    MemberService_Profile profileService_;
    MemberService_Experience experienceService_;
};

}  // namespace auth

