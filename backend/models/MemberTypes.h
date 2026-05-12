#pragma once

#include <cstdint>
#include <optional>
#include <string>

namespace auth
{

// 로그인 요청 본문 DTO.
class LoginRequestDTO
{
  public:
    std::string email;
    std::string password;
};

// 회원가입 요청 본문 DTO.
class SignupRequestDTO
{
  public:
    std::string name;
    std::string email;
    std::string password;
    std::string confirmPassword;
    bool agreeTerms{false};
};

// members 테이블 한 행을 그대로 담는 내부 모델.
// passwordHash가 포함되므로 외부 응답으로 직접 노출하면 안 된다.
class MemberModel
{
  public:
    std::uint64_t memberId{0};
    std::string email;
    std::string passwordHash;
    std::string name;
    std::string role;
    std::string status;
    unsigned int level{1};
    unsigned int exp{0};
    std::optional<std::string> lastLoginAt;
};

// 클라이언트 응답 전용 회원 DTO.
// 보안상 passwordHash를 제외한 필드만 유지한다.
class MemberDTO
{
  public:
    std::uint64_t memberId{0};
    std::string email;
    std::string name;
    std::string role;
    std::string status;
    unsigned int level{1};
    unsigned int exp{0};
    std::optional<std::string> lastLoginAt;
};

// 로그인/회원가입 서비스 결과 래퍼 DTO.
class AuthResultDTO
{
  public:
    bool ok{false};
    int statusCode{400};
    std::string message;
    std::optional<MemberDTO> member;
    std::optional<std::string> sessionToken;
};

// 경험치 지급 요청 DTO.
class AwardExperienceRequestDTO
{
  public:
    std::string actionType;
};

// 경험치 지급 서비스 결과 DTO.
class AwardExperienceResultDTO
{
  public:
    bool ok{false};
    int statusCode{400};
    std::string message;
    unsigned int awardedExp{0};
    unsigned int previousLevel{1};
    unsigned int newLevel{1};
    std::optional<MemberDTO> member;
};

}  // namespace auth
