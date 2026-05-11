#pragma once

#include <chrono>
#include <mutex>
#include <optional>
#include <string>
#include <unordered_map>

#include "../mappers/MemberMapper.h"
#include "../models/MemberTypes.h"

namespace auth
{

// 인증 도메인의 비즈니스 규칙을 담당한다.
// Controller는 HTTP를 처리하고, 이 클래스는 검증/비밀번호/세션/DB 호출 순서를 책임진다.
class MemberService
{
  public:
    MemberService() = default;

    // 회원가입 입력값을 검증하고 members 테이블에 사용자를 만든 뒤 로그인 세션을 생성한다.
    AuthResultDTO signup(const SignupRequestDTO &request);
    // 이메일/비밀번호를 검증하고 성공 시 로그인 세션을 생성한다.
    AuthResultDTO login(const LoginRequestDTO &request);
    // 세션 토큰이 유효하면 현재 로그인한 회원 정보를 반환한다.
    std::optional<MemberDTO> getCurrentMember(const std::string &sessionToken);
    // 세션 저장소에서 토큰을 제거해 로그아웃 처리한다.
    bool logout(const std::string &sessionToken);
    // 활동 타입에 맞는 경험치를 지급하고 레벨/경험치를 DB에 반영한다.
    AwardExperienceResultDTO awardExperience(
        const std::string &sessionToken,
        const AwardExperienceRequestDTO &request);

  private:
    // 현재 구현은 서버 메모리에 세션을 저장하므로 토큰별 회원 id와 만료 시간을 함께 들고 있다.
    struct SessionData
    {
        std::uint64_t memberId{0};
        std::chrono::system_clock::time_point expiresAt;
    };

    // 사용자 입력의 앞뒤 공백을 제거한다.
    static std::string trim(std::string value);
    // 이메일 비교를 안정적으로 하기 위해 문자열을 소문자로 정규화한다.
    static std::string toLower(std::string value);
    // 최소 형식의 이메일인지 확인한다.
    static bool isValidEmail(const std::string &email);
    // salt와 session token에 사용할 임의의 16진수 문자열을 만든다.
    static std::string randomHex(std::size_t length);
    // 비밀번호를 salt와 함께 저장 가능한 문자열로 만든다.
    static std::string hashPasswordWithSalt(const std::string &password);
    // 입력 비밀번호가 저장된 hash 문자열과 일치하는지 확인한다.
    static bool verifyPassword(const std::string &password,
                               const std::string &storedHash);
    // 프론트가 보낸 활동 타입을 서버 기준 경험치 값으로 변환한다.
    static std::optional<unsigned int> resolveAwardedExpByActionType(
        const std::string &actionType);

    // 로그인 상태를 유지할 새 세션 토큰을 만들고 메모리 저장소에 등록한다.
    std::string createSession(std::uint64_t memberId);
    // 세션 토큰을 회원 id로 해석한다. 없거나 만료된 토큰이면 nullopt를 반환한다.
    std::optional<std::uint64_t> resolveSessionMemberId(
        const std::string &sessionToken);
    // sessionsMutex_를 잡은 상태에서 만료된 세션을 정리한다.
    void cleanupExpiredSessionsLocked(
        const std::chrono::system_clock::time_point &now);

    // 회원 데이터 조회/생성/수정은 mapper 계층에 위임한다.
    MemberMapper mapper_;
    // 간단한 로컬 세션 저장소다. 서버 재시작 시 모든 세션은 사라진다.
    std::unordered_map<std::string, SessionData> sessions_;
    // 여러 요청이 동시에 세션 저장소를 수정할 수 있으므로 mutex로 보호한다.
    std::mutex sessionsMutex_;
};

}  // namespace auth
