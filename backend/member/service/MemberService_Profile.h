/*
 * 파일 개요: 프로필 조회/수정 흐름을 묶는 상위 서비스 인터페이스다.
 * 주요 역할: 현재 사용자 조회, 내/타인 프로필 조회, 프로필 변경 API 제공.
 */
#pragma once

#include <cstdint>
#include <optional>
#include <string>

#include "friend/mapper/FriendMapper.h"
#include "member/mapper/MemberMapper.h"
#include "member/model/MemberTypes.h"
#include "member/service/MemberService_Profile_Command.h"
#include "member/service/MemberService_Profile_Query.h"
#include "member/service/MemberService_SessionStore.h"

namespace auth
{

// 프로필 도메인의 읽기/쓰기 흐름을 묶는 조정자 서비스다.
class MemberService_Profile
{
  public:
    MemberService_Profile(MemberMapper &mapper,
                          friendship::FriendMapper &friendMapper,
                          MemberService_SessionStore &sessionStore);

    // 세션 토큰 기준 현재 로그인 사용자의 기본 정보를 조회한다.
    // 인증 실패 시 nullopt를 반환한다.
    std::optional<MemberDTO> getCurrentMember(const std::string &sessionToken);

    // 세션 토큰 기준 현재 사용자 ID만 조회한다.
    // 다른 서비스에서 권한 판별용으로 빠르게 사용한다.
    std::optional<std::uint64_t> getCurrentMemberId(const std::string &sessionToken);

    // 프로필 수정(이름/이메일)을 수행한다.
    UpdateProfileResultDTO updateProfile(const std::string &sessionToken,
                                         const UpdateProfileRequestDTO &request);

    // 내 Food MBTI 결과를 저장한다.
    // 없으면 생성, 있으면 갱신하는 업서트 흐름을 따른다.
    SaveFoodMbtiResultDTO saveMyFoodMbti(
        const std::string &sessionToken,
        const SaveFoodMbtiRequestDTO &request);

    // 내 프로필 상세를 조회한다.
    MemberProfileResultDTO getMyProfile(const std::string &sessionToken);

    // 타인 프로필 상세를 조회한다.
    // Query 계층에서 친구관계/본인 여부에 따른 필드 노출 정책을 적용한다.
    MemberProfileResultDTO getMemberProfile(const std::string &sessionToken,
                                            std::uint64_t targetMemberId);

  private:
    // query_: 읽기 전용 로직, command_: 쓰기 전용 로직
    MemberService_Profile_Query query_;
    MemberService_Profile_Command command_;
};

}  // namespace auth

