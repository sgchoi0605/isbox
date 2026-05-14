/*
 * 파일 개요: 프로필 읽기(Query) 전용 서비스 인터페이스다.
 * 주요 역할: 현재 사용자/내 프로필/타인 프로필 조회 계약 제공.
 */
#pragma once

#include <cstdint>
#include <optional>
#include <string>

#include "friend/mapper/FriendMapper.h"
#include "member/mapper/MemberMapper.h"
#include "member/model/MemberTypes.h"
#include "member/service/MemberService_SessionStore.h"

namespace auth
{

// 프로필 읽기 계열(조회) 쿼리 서비스를 담당한다.
class MemberService_Profile_Query
{
  public:
    MemberService_Profile_Query(MemberMapper &mapper,
                                friendship::FriendMapper &friendMapper,
                                MemberService_SessionStore &sessionStore)
        : mapper_(mapper), friendMapper_(friendMapper), sessionStore_(sessionStore)
    {
    }

    // 세션 토큰에 해당하는 현재 사용자 기본 정보를 조회한다.
    // 토큰이 만료되었거나 사용자 미존재 시 nullopt를 반환한다.
    std::optional<MemberDTO> getCurrentMember(const std::string &sessionToken);

    // 세션 토큰의 사용자 ID만 반환한다.
    // 다른 조회 경로에서 권한/본인 여부 판단의 공통 입력으로 사용된다.
    std::optional<std::uint64_t> getCurrentMemberId(const std::string &sessionToken);

    // 현재 로그인 사용자의 프로필 상세를 조회한다.
    MemberProfileResultDTO getMyProfile(const std::string &sessionToken);

    // targetMemberId 프로필을 조회한다.
    // friendMapper를 이용해 관계 정보를 반영한 응답을 구성할 수 있다.
    MemberProfileResultDTO getMemberProfile(const std::string &sessionToken,
                                            std::uint64_t targetMemberId);

  private:
    // mapper_: 회원/MBTI 원천 데이터 조회
    // friendMapper_: 관계 기반 부가 정보 조회
    // sessionStore_: 세션 인증 및 현재 사용자 판별
    MemberMapper &mapper_;
    friendship::FriendMapper &friendMapper_;
    MemberService_SessionStore &sessionStore_;
};

}  // namespace auth

