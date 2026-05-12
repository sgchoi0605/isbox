#pragma once

#include <chrono>
#include <mutex>
#include <optional>
#include <string>
#include <unordered_map>

#include "../mappers/FriendMapper.h"
#include "../mappers/MemberMapper.h"
#include "../models/MemberTypes.h"

namespace auth
{

// 회원 인증과 계정 로직을 담당하는 서비스 계층이다.
// 검증, 비밀번호, 세션 규칙을 처리하고 DB 입출력은 매퍼에 위임한다.
class MemberService
{
  public:
    MemberService() = default;

    // 회원가입 입력을 검증하고 회원을 생성한 뒤 로그인 세션 토큰을 반환한다.
    AuthResultDTO signup(const SignupRequestDTO &request);

    // 로그인 정보를 검증하고 세션 토큰을 반환한다.
    AuthResultDTO login(const LoginRequestDTO &request);

    // 세션 토큰을 해석해 활성 회원 정보를 반환한다.
    std::optional<MemberDTO> getCurrentMember(const std::string &sessionToken);
    std::optional<std::uint64_t> getCurrentMemberId(
        const std::string &sessionToken);

    UpdateProfileResultDTO updateProfile(const std::string &sessionToken,
                                         const UpdateProfileRequestDTO &request);

    ChangePasswordResultDTO changePassword(
        const std::string &sessionToken,
        const ChangePasswordRequestDTO &request);

    // 세션 토큰을 무효화한다.
    bool logout(const std::string &sessionToken);

    // 유효한 활동 유형에 경험치를 지급하고 레벨 진행 상태를 갱신한다.
    AwardExperienceResultDTO awardExperience(
        const std::string &sessionToken,
        const AwardExperienceRequestDTO &request);

    SaveFoodMbtiResultDTO saveMyFoodMbti(
        const std::string &sessionToken,
        const SaveFoodMbtiRequestDTO &request);

    MemberProfileResultDTO getMyProfile(const std::string &sessionToken);

    MemberProfileResultDTO getMemberProfile(const std::string &sessionToken,
                                            std::uint64_t targetMemberId);

  private:
    // 메모리에 저장되는 세션 정보다.
    struct SessionData
    {
        std::uint64_t memberId{0};
        std::chrono::system_clock::time_point expiresAt;
    };

    // 문자열 앞뒤 공백을 제거한다.
    static std::string trim(std::string value);

    // 문자열을 소문자로 변환한다.
    static std::string toLower(std::string value);

    // 최소한의 이메일 형식을 확인한다.
    static bool isValidEmail(const std::string &email);

    // 임의의 소문자 16진수 문자열을 생성한다.
    static std::string randomHex(std::size_t length);

    // 임의 솔트로 비밀번호를 해시해 "salt$hash" 형식으로 반환한다.
    static std::string hashPasswordWithSalt(const std::string &password);

    // 입력 비밀번호가 저장된 해시 문자열과 일치하는지 검증한다.
    static bool verifyPassword(const std::string &password,
                               const std::string &storedHash);

    // 활동 유형을 지급 경험치 값으로 변환한다.
    static std::optional<unsigned int> resolveAwardedExpByActionType(
        const std::string &actionType);

    // 세션 토큰을 생성하고 만료 세션을 정리한 뒤 새 세션을 저장한다.
    std::string createSession(std::uint64_t memberId);

    // 유효한 세션 토큰을 회원 id로 변환한다.
    std::optional<std::uint64_t> resolveSessionMemberId(
        const std::string &sessionToken);

    // 만료된 세션을 제거한다.
    // 호출자는 이미 sessionsMutex_를 잡고 있어야 한다.
    void cleanupExpiredSessionsLocked(
        const std::chrono::system_clock::time_point &now);

    // 데이터 접근 객체다.
    MemberMapper mapper_;
    friendship::FriendMapper friendMapper_;

    // 메모리 기반 토큰 저장소다. 토큰에서 세션 정보로 이어지는 형태로 저장한다.
    std::unordered_map<std::string, SessionData> sessions_;

    // sessions_에 대한 동시 접근을 보호한다.
    std::mutex sessionsMutex_;
};

}  // 인증 네임스페이스
