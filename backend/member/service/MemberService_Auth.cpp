/*
 * 파일 개요: 회원가입/로그인/비밀번호 변경 인증 로직 구현 파일이다.
 * 주요 역할: 입력 검증, 비밀번호 해시 검증, 세션 발급, 인증 관련 오류 처리.
 */
#include "member/service/MemberService_Auth.h"

#include <drogon/drogon.h>
#include <drogon/orm/Exception.h>

#include <algorithm>
#include <array>
#include <cctype>
#include <functional>
#include <random>

namespace
{

// 로그인 실패 시 계정 존재 여부를 노출하지 않기 위한 공통 메시지다.
std::string makeGenericLoginErrorMessage()
{
    return "Invalid email or password.";
}

// DB 예외 메시지에서 이메일 중복 제약 위반을 판별한다.
bool looksLikeDuplicateEmail(const std::string &message)
{
    std::string lowered = message;
    std::transform(lowered.begin(), lowered.end(), lowered.begin(), [](unsigned char ch) {
        return static_cast<char>(std::tolower(ch));
    });

    return lowered.find("duplicate") != std::string::npos ||
           lowered.find("uq_members_email") != std::string::npos;
}

}  // namespace

namespace auth
{

std::string MemberService_Auth::trim(std::string value)
{
    // 입력 양끝 공백을 제거한다.
    const auto notSpace = [](unsigned char ch) { return !std::isspace(ch); };
    value.erase(value.begin(), std::find_if(value.begin(), value.end(), notSpace));
    value.erase(std::find_if(value.rbegin(), value.rend(), notSpace).base(), value.end());
    return value;
}

std::string MemberService_Auth::toLower(std::string value)
{
    // 이메일 비교를 위해 소문자로 정규화한다.
    std::transform(value.begin(), value.end(), value.begin(), [](unsigned char ch) {
        return static_cast<char>(std::tolower(ch));
    });
    return value;
}

bool MemberService_Auth::isValidEmail(const std::string &email)
{
    // 간단한 구조(@, 도메인 점) 검증만 수행한다.
    const auto atPos = email.find('@');
    if (atPos == std::string::npos || atPos == 0 || atPos + 1 >= email.size())
    {
        return false;
    }

    const auto dotPos = email.find('.', atPos + 1);
    return dotPos != std::string::npos && dotPos + 1 < email.size();
}

std::string MemberService_Auth::randomHex(std::size_t length)
{
    // 난수 기반 hex 문자열을 생성한다.
    static constexpr std::array<char, 16> kHexChars{
        '0', '1', '2', '3', '4', '5', '6', '7',
        '8', '9', 'a', 'b', 'c', 'd', 'e', 'f'};

    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<int> dist(0, 15);

    std::string out;
    out.reserve(length);
    for (std::size_t i = 0; i < length; ++i)
    {
        out.push_back(kHexChars[static_cast<std::size_t>(dist(gen))]);
    }
    return out;
}

std::string MemberService_Auth::hashPasswordWithSalt(const std::string &password)
{
    // salt + password 해시를 "salt$hash" 형태로 저장한다.
    const auto salt = randomHex(16);
    const auto hash =
        std::to_string(std::hash<std::string>{}(salt + "::" + password));
    return salt + "$" + hash;
}

bool MemberService_Auth::verifyPassword(const std::string &password,
                                        const std::string &storedHash)
{
    // 구버전 호환: 구분자($)가 없으면 평문 비교를 수행한다.
    const auto separatorPos = storedHash.find('$');
    if (separatorPos == std::string::npos)
    {
        return storedHash == password;
    }

    const auto salt = storedHash.substr(0, separatorPos);
    const auto expectedHash = storedHash.substr(separatorPos + 1);
    const auto actualHash =
        std::to_string(std::hash<std::string>{}(salt + "::" + password));
    return expectedHash == actualHash;
}

AuthResultDTO MemberService_Auth::signup(const SignupRequestDTO &request)
{
    // 회원가입: 검증 -> 중복확인 -> 생성 -> 세션발급 순서로 처리한다.
    AuthResultDTO result;

    const auto name = trim(request.name);
    const auto email = toLower(trim(request.email));
    const auto password = request.password;
    const auto confirmPassword = request.confirmPassword;

    // 입력 검증
    if (name.empty() || email.empty() || password.empty() || confirmPassword.empty())
    {
        result.statusCode = 400;
        result.message = "Please fill in all required fields.";
        return result;
    }

    if (!request.agreeTerms)
    {
        result.statusCode = 400;
        result.message = "You must agree to the terms.";
        return result;
    }

    if (!isValidEmail(email))
    {
        result.statusCode = 400;
        result.message = "Invalid email format.";
        return result;
    }

    if (password.size() < 8U)
    {
        result.statusCode = 400;
        result.message = "Password must be at least 8 characters.";
        return result;
    }

    if (password != confirmPassword)
    {
        result.statusCode = 400;
        result.message = "Passwords do not match.";
        return result;
    }

    try
    {
        if (mapper_.findByEmail(email).has_value())
        {
            result.statusCode = 409;
            result.message = "Email is already registered.";
            return result;
        }

        const auto passwordHash = hashPasswordWithSalt(password);
        auto createdMember = mapper_.createMember(email, passwordHash, name);
        if (!createdMember.has_value())
        {
            result.statusCode = 500;
            result.message = "Failed to create member.";
            return result;
        }

        mapper_.updateLastLoginAt(createdMember->memberId);
        createdMember = mapper_.findById(createdMember->memberId);
        if (!createdMember.has_value())
        {
            result.statusCode = 500;
            result.message = "Failed to load member.";
            return result;
        }

        const auto sessionToken = sessionStore_.createSession(createdMember->memberId);

        result.ok = true;
        result.statusCode = 201;
        result.message = "Signup success.";
        result.member = mapper_.toMemberDTO(*createdMember);
        result.sessionToken = sessionToken;
        return result;
    }
    catch (const drogon::orm::DrogonDbException &e)
    {
        LOG_ERROR << "signup db error: " << e.base().what();

        if (looksLikeDuplicateEmail(e.base().what()))
        {
            result.statusCode = 409;
            result.message = "Email is already registered.";
            return result;
        }

        result.statusCode = 500;
        result.message = "Database error during signup.";
        return result;
    }
    catch (const std::exception &)
    {
        result.statusCode = 500;
        result.message = "Server error during signup.";
        return result;
    }
}

AuthResultDTO MemberService_Auth::login(const LoginRequestDTO &request)
{
    // 로그인: 계정조회 -> 상태검증 -> 비밀번호검증 -> 세션발급을 수행한다.
    AuthResultDTO result;
    const auto email = toLower(trim(request.email));
    const auto password = request.password;

    if (email.empty() || password.empty())
    {
        result.statusCode = 400;
        result.message = "Please enter email and password.";
        return result;
    }

    try
    {
        auto member = mapper_.findByEmail(email);
        if (!member.has_value())
        {
            result.statusCode = 401;
            result.message = makeGenericLoginErrorMessage();
            return result;
        }

        if (member->status != "ACTIVE")
        {
            result.statusCode = 401;
            result.message = makeGenericLoginErrorMessage();
            return result;
        }

        if (!verifyPassword(password, member->passwordHash))
        {
            result.statusCode = 401;
            result.message = makeGenericLoginErrorMessage();
            return result;
        }

        mapper_.updateLastLoginAt(member->memberId);
        member = mapper_.findById(member->memberId);
        if (!member.has_value())
        {
            result.statusCode = 500;
            result.message = "Failed to load member.";
            return result;
        }

        const auto sessionToken = sessionStore_.createSession(member->memberId);

        result.ok = true;
        result.statusCode = 200;
        result.message = "Login success.";
        result.member = mapper_.toMemberDTO(*member);
        result.sessionToken = sessionToken;
        return result;
    }
    catch (const std::exception &)
    {
        result.statusCode = 500;
        result.message = "Server error during login.";
        return result;
    }
}

ChangePasswordResultDTO MemberService_Auth::changePassword(
    const std::string &sessionToken,
    const ChangePasswordRequestDTO &request)
{
    // 비밀번호 변경: 인증 사용자만 현재 비밀번호 검증 후 갱신한다.
    ChangePasswordResultDTO result;

    const auto currentPassword = request.currentPassword;
    const auto newPassword = request.newPassword;
    const auto confirmPassword = request.confirmPassword;

    if (currentPassword.empty() || newPassword.empty() || confirmPassword.empty())
    {
        result.statusCode = 400;
        result.message = "Please fill in all required fields.";
        return result;
    }

    if (newPassword.size() < 8U)
    {
        result.statusCode = 400;
        result.message = "Password must be at least 8 characters.";
        return result;
    }

    if (newPassword != confirmPassword)
    {
        result.statusCode = 400;
        result.message = "Passwords do not match.";
        return result;
    }

    if (currentPassword == newPassword)
    {
        result.statusCode = 400;
        result.message = "New password must be different.";
        return result;
    }

    try
    {
        const auto memberId = sessionStore_.resolveSessionMemberId(sessionToken);
        if (!memberId.has_value())
        {
            result.statusCode = 401;
            result.message = "Unauthorized.";
            return result;
        }

        const auto member = mapper_.findById(*memberId);
        if (!member.has_value() || member->status != "ACTIVE")
        {
            result.statusCode = 401;
            result.message = "Unauthorized.";
            return result;
        }

        if (!verifyPassword(currentPassword, member->passwordHash))
        {
            result.statusCode = 401;
            result.message = "Current password is incorrect.";
            return result;
        }

        mapper_.updatePasswordHash(*memberId, hashPasswordWithSalt(newPassword));

        result.ok = true;
        result.statusCode = 200;
        result.message = "Password updated.";
        return result;
    }
    catch (const std::exception &)
    {
        result.statusCode = 500;
        result.message = "Server error while updating password.";
        return result;
    }
}

}  // namespace auth

