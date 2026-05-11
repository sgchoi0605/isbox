#pragma once

#include <cstdint>
#include <optional>
#include <string>

namespace auth
{

// 로그인 요청 body에서 필요한 값만 담는 DTO다.
class LoginRequestDTO
{
public:
    std::string email;
    std::string password;
};

// 회원가입 요청 body에서 필요한 값만 담는 DTO다.
class SignupRequestDTO
{
public:
    std::string name;
    std::string email;
    std::string password;
    std::string confirmPassword;
    bool agreeTerms{false};
};

// DB의 members row를 거의 그대로 담는 내부 모델이다.
// passwordHash가 포함되므로 Controller 응답으로 직접 내보내면 안 된다.
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

// 프론트엔드에 내려줄 수 있는 회원 정보 DTO다.
// 비밀번호 hash는 의도적으로 제외한다.
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

// 로그인/회원가입 서비스 결과를 Controller가 HTTP 응답으로 바꿀 때 쓰는 DTO다.
class AuthResultDTO
{
public:
    bool ok{false};
    int statusCode{400};
    std::string message;
    std::optional<MemberDTO> member;
    std::optional<std::string> sessionToken;
};

// 경험치 지급 요청 DTO다.
class AwardExperienceRequestDTO
{
public:
    std::string actionType;
};

// 경험치 지급 서비스 결과 DTO다.
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
