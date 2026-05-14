/*
 * 파일 개요: 프로필 도메인 조정자 서비스 구현 파일이다.
 * 주요 역할: 조회(Query)와 변경(Command) 경로를 분리된 객체에 위임해 조합.
 */
#include "member/service/MemberService_Profile.h"

namespace auth
{

MemberService_Profile::MemberService_Profile(
    MemberMapper &mapper,
    friendship::FriendMapper &friendMapper,
    MemberService_SessionStore &sessionStore)
    : query_(mapper, friendMapper, sessionStore), command_(mapper, sessionStore)
{
    // 조회/쓰기 책임을 각각 Query/Command 객체로 분리해 조합한다.
}

std::optional<MemberDTO> MemberService_Profile::getCurrentMember(
    const std::string &sessionToken)
{
    // 읽기 전용 조회는 Query로 위임한다.
    return query_.getCurrentMember(sessionToken);
}

std::optional<std::uint64_t> MemberService_Profile::getCurrentMemberId(
    const std::string &sessionToken)
{
    // 토큰에서 현재 사용자 ID만 빠르게 조회한다.
    return query_.getCurrentMemberId(sessionToken);
}

UpdateProfileResultDTO MemberService_Profile::updateProfile(
    const std::string &sessionToken,
    const UpdateProfileRequestDTO &request)
{
    // 변경 작업은 Command로 위임한다.
    return command_.updateProfile(sessionToken, request);
}

SaveFoodMbtiResultDTO MemberService_Profile::saveMyFoodMbti(
    const std::string &sessionToken,
    const SaveFoodMbtiRequestDTO &request)
{
    // MBTI 저장도 쓰기 작업이므로 Command로 위임한다.
    return command_.saveMyFoodMbti(sessionToken, request);
}

MemberProfileResultDTO MemberService_Profile::getMyProfile(
    const std::string &sessionToken)
{
    // 내 프로필 조회는 Query에서 수행한다.
    return query_.getMyProfile(sessionToken);
}

MemberProfileResultDTO MemberService_Profile::getMemberProfile(
    const std::string &sessionToken,
    std::uint64_t targetMemberId)
{
    // 타인 프로필 조회 권한 검증도 Query에서 처리한다.
    return query_.getMemberProfile(sessionToken, targetMemberId);
}

}  // namespace auth

