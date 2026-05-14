/*
 * 파일 개요: 회원 도메인 통합 서비스 파사드의 위임 구현 파일이다.
 * 주요 역할: 컨트롤러 요청을 하위 Auth/Profile/Experience 서비스로 라우팅.
 */
#include "member/service/MemberService.h"

namespace auth
{

MemberService::MemberService()
    : authService_(mapper_, sessionStore_),
      profileService_(mapper_, friendMapper_, sessionStore_),
      experienceService_(mapper_, sessionStore_)
{
    // 서브 서비스 간 의존성을 한 번에 조립한다.
}

AuthResultDTO MemberService::signup(const SignupRequestDTO &request)
{
    // 회원가입 로직은 인증 서비스로 위임한다.
    return authService_.signup(request);
}

AuthResultDTO MemberService::login(const LoginRequestDTO &request)
{
    return authService_.login(request);
}

std::optional<MemberDTO> MemberService::getCurrentMember(const std::string &sessionToken)
{
    // 현재 사용자 판별은 프로필 조회 모듈에 위임한다.
    return profileService_.getCurrentMember(sessionToken);
}

std::optional<std::uint64_t> MemberService::getCurrentMemberId(
    const std::string &sessionToken)
{
    return profileService_.getCurrentMemberId(sessionToken);
}

UpdateProfileResultDTO MemberService::updateProfile(
    const std::string &sessionToken,
    const UpdateProfileRequestDTO &request)
{
    return profileService_.updateProfile(sessionToken, request);
}

ChangePasswordResultDTO MemberService::changePassword(
    const std::string &sessionToken,
    const ChangePasswordRequestDTO &request)
{
    // 비밀번호 변경은 인증 모듈에서 처리한다.
    return authService_.changePassword(sessionToken, request);
}

bool MemberService::logout(const std::string &sessionToken)
{
    // 세션 저장소에서 토큰을 제거한다.
    return sessionStore_.removeSession(sessionToken);
}

AwardExperienceResultDTO MemberService::awardExperience(
    const std::string &sessionToken,
    const AwardExperienceRequestDTO &request)
{
    // 경험치 규칙 계산/반영은 경험치 모듈에 위임한다.
    return experienceService_.awardExperience(sessionToken, request);
}

SaveFoodMbtiResultDTO MemberService::saveMyFoodMbti(
    const std::string &sessionToken,
    const SaveFoodMbtiRequestDTO &request)
{
    return profileService_.saveMyFoodMbti(sessionToken, request);
}

MemberProfileResultDTO MemberService::getMyProfile(const std::string &sessionToken)
{
    return profileService_.getMyProfile(sessionToken);
}

MemberProfileResultDTO MemberService::getMemberProfile(
    const std::string &sessionToken,
    std::uint64_t targetMemberId)
{
    return profileService_.getMemberProfile(sessionToken, targetMemberId);
}

}  // namespace auth

