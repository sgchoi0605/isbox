#include "MemberService.h"

#include <drogon/drogon.h>
#include <drogon/orm/Exception.h>

#include <algorithm>
#include <array>
#include <cctype>
#include <functional>
#include <random>

namespace
{

// 세션 유효 시간은 24시간이다.
constexpr int kSessionDurationSeconds = 60 * 60 * 24;

std::string makeGenericLoginErrorMessage()
{
    // 계정 존재 여부가 노출되지 않도록 모든 로그인 실패에 같은 메시지를 사용한다.
    return "Invalid email or password.";
}

bool looksLikeDuplicateEmail(const std::string &message)
{
    // 중복 키 오류를 느슨하게 감지하기 위해 DB 오류 메시지를 정규화한다.
    std::string lowered = message;
    std::transform(lowered.begin(), lowered.end(), lowered.begin(), [](unsigned char ch) {
        return static_cast<char>(std::tolower(ch));
    });

    return lowered.find("duplicate") != std::string::npos ||
           lowered.find("uq_members_email") != std::string::npos;
}

}  // 익명 네임스페이스

namespace auth
{

AuthResultDTO MemberService::signup(const SignupRequestDTO &request)
{
    AuthResultDTO result;

    // 검증 전에 사용자 입력 필드를 정규화한다.
    const auto name = trim(request.name);
    const auto email = toLower(trim(request.email));
    const auto password = request.password;
    const auto confirmPassword = request.confirmPassword;

    // 필수 입력값이 모두 있어야 한다.
    if (name.empty() || email.empty() || password.empty() || confirmPassword.empty())
    {
        result.statusCode = 400;
        result.message = "Please fill in all required fields.";
        return result;
    }

    // 약관 동의는 필수다.
    if (!request.agreeTerms)
    {
        result.statusCode = 400;
        result.message = "You must agree to the terms.";
        return result;
    }

    // 최소한의 이메일 형식을 확인한다.
    if (!isValidEmail(email))
    {
        result.statusCode = 400;
        result.message = "Invalid email format.";
        return result;
    }

    // 비밀번호 최소 길이를 강제한다.
    if (password.size() < 8U)
    {
        result.statusCode = 400;
        result.message = "Password must be at least 8 characters.";
        return result;
    }

    // 비밀번호 확인값은 원본 비밀번호와 일치해야 한다.
    if (password != confirmPassword)
    {
        result.statusCode = 400;
        result.message = "Passwords do not match.";
        return result;
    }

    try
    {
        // 명확한 409 응답을 위해 이메일 중복 여부를 먼저 확인한다.
        if (mapper_.findByEmail(email).has_value())
        {
            result.statusCode = 409;
            result.message = "Email is already registered.";
            return result;
        }

        // 평문 비밀번호는 저장하지 않는다.
        const auto passwordHash = hashPasswordWithSalt(password);
        auto createdMember = mapper_.createMember(email, passwordHash, name);

        if (!createdMember.has_value())
        {
            result.statusCode = 500;
            result.message = "Failed to create member.";
            return result;
        }

        // 회원가입 직후 로그인된 상태가 되도록 마지막 로그인 시간을 갱신한다.
        mapper_.updateLastLoginAt(createdMember->memberId);
        createdMember = mapper_.findById(createdMember->memberId);
        if (!createdMember.has_value())
        {
            result.statusCode = 500;
            result.message = "Failed to load member.";
            return result;
        }

        // 세션 토큰을 발급하고 메모리 세션 저장소에 기록한다.
        const auto sessionToken = createSession(createdMember->memberId);

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

        // 동시 요청으로 DB 고유 제약에 걸리는 경우를 처리한다.
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

AuthResultDTO MemberService::login(const LoginRequestDTO &request)
{
    AuthResultDTO result;

    // 안정적인 비교를 위해 이메일을 정규화한다.
    const auto email = toLower(trim(request.email));
    const auto password = request.password;

    // 이메일과 비밀번호는 모두 필수다.
    if (email.empty() || password.empty())
    {
        result.statusCode = 400;
        result.message = "Please enter email and password.";
        return result;
    }

    try
    {
        auto member = mapper_.findByEmail(email);

        // 계정이 없어도 일반 인증 실패 메시지를 반환한다.
        if (!member.has_value())
        {
            result.statusCode = 401;
            result.message = makeGenericLoginErrorMessage();
            return result;
        }

        // 활성 상태가 아닌 계정은 로그인할 수 없다.
        if (member->status != "ACTIVE")
        {
            result.statusCode = 401;
            result.message = makeGenericLoginErrorMessage();
            return result;
        }

        // 저장된 솔트/해시 값으로 비밀번호를 검증한다.
        if (!verifyPassword(password, member->passwordHash))
        {
            result.statusCode = 401;
            result.message = makeGenericLoginErrorMessage();
            return result;
        }

        // 마지막 로그인 시간을 갱신하고 최신 회원 정보를 다시 조회한다.
        mapper_.updateLastLoginAt(member->memberId);
        member = mapper_.findById(member->memberId);
        if (!member.has_value())
        {
            result.statusCode = 500;
            result.message = "Failed to load member.";
            return result;
        }

        // 이후 인증 요청에 사용할 세션 토큰을 발급한다.
        const auto sessionToken = createSession(member->memberId);

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

std::optional<MemberDTO> MemberService::getCurrentMember(
    const std::string &sessionToken)
{
    try
    {
        // 세션 토큰을 회원 id로 변환한다.
        const auto memberId = resolveSessionMemberId(sessionToken);
        if (!memberId.has_value())
        {
            return std::nullopt;
        }

        // 삭제되었거나 비활성화된 계정은 인증 상태로 인정하지 않는다.
        const auto member = mapper_.findById(*memberId);
        if (!member.has_value() || member->status != "ACTIVE")
        {
            return std::nullopt;
        }

        return mapper_.toMemberDTO(*member);
    }
    catch (const std::exception &)
    {
        return std::nullopt;
    }
}

bool MemberService::logout(const std::string &sessionToken)
{
    if (sessionToken.empty())
    {
        return false;
    }

    // 세션 저장소는 여러 요청이 공유하므로 뮤텍스로 보호한다.
    std::lock_guard<std::mutex> lock(sessionsMutex_);
    return sessions_.erase(sessionToken) > 0;
}

AwardExperienceResultDTO MemberService::awardExperience(
    const std::string &sessionToken,
    const AwardExperienceRequestDTO &request)
{
    AwardExperienceResultDTO result;

    // 활동 유형을 설정된 경험치 값으로 변환한다.
    const auto awardedExp = resolveAwardedExpByActionType(request.actionType);
    if (!awardedExp.has_value())
    {
        result.statusCode = 400;
        result.message = "Invalid action type.";
        return result;
    }

    try
    {
        // 세션 토큰으로 요청자를 인증한다.
        const auto memberId = resolveSessionMemberId(sessionToken);
        if (!memberId.has_value())
        {
            result.statusCode = 401;
            result.message = "Unauthorized.";
            return result;
        }

        // 대상 회원은 여전히 활성 상태여야 한다.
        auto member = mapper_.findById(*memberId);
        if (!member.has_value() || member->status != "ACTIVE")
        {
            result.statusCode = 401;
            result.message = "Unauthorized.";
            return result;
        }

        const auto previousLevel = member->level;
        std::uint64_t accumulatedExp =
            static_cast<std::uint64_t>(member->exp) + *awardedExp;
        unsigned int newLevel = member->level;

        // 레벨업 규칙은 N레벨에서 필요한 경험치가 N * 100이라는 것이다.
        while (true)
        {
            const std::uint64_t requiredExp =
                static_cast<std::uint64_t>(newLevel) * 100ULL;
            if (accumulatedExp < requiredExp)
            {
                break;
            }
            accumulatedExp -= requiredExp;
            ++newLevel;
        }

        const auto newExp = static_cast<unsigned int>(accumulatedExp);
        mapper_.updateLevelAndExp(*memberId, newLevel, newExp);

        // 응답에 최신 상태를 담기 위해 다시 조회한다.
        member = mapper_.findById(*memberId);
        if (!member.has_value())
        {
            result.statusCode = 500;
            result.message = "Failed to load member.";
            return result;
        }

        result.ok = true;
        result.statusCode = 200;
        result.message = "Experience updated.";
        result.awardedExp = *awardedExp;
        result.previousLevel = previousLevel;
        result.newLevel = member->level;
        result.member = mapper_.toMemberDTO(*member);
        return result;
    }
    catch (const std::exception &)
    {
        result.statusCode = 500;
        result.message = "Server error while updating experience.";
        return result;
    }
}

std::optional<unsigned int> MemberService::resolveAwardedExpByActionType(
    const std::string &actionType)
{
    const auto normalized = toLower(trim(actionType));

    // 활동 유형별 경험치 매핑이다.
    if (normalized == "ingredient_add")
    {
        return 10U;
    }
    if (normalized == "recipe_recommend")
    {
        return 20U;
    }
    if (normalized == "community_post_create")
    {
        return 30U;
    }
    if (normalized == "food_mbti_complete")
    {
        return 50U;
    }

    return std::nullopt;
}

std::string MemberService::trim(std::string value)
{
    // 문자열 앞뒤 공백을 제거하는 공용 유틸리티다.
    const auto notSpace = [](unsigned char ch) { return !std::isspace(ch); };

    value.erase(value.begin(),
                std::find_if(value.begin(), value.end(), notSpace));
    value.erase(
        std::find_if(value.rbegin(), value.rend(), notSpace).base(), value.end());

    return value;
}

std::string MemberService::toLower(std::string value)
{
    // 대소문자 구분 없는 비교를 위해 정규화한다.
    std::transform(value.begin(), value.end(), value.begin(), [](unsigned char ch) {
        return static_cast<char>(std::tolower(ch));
    });
    return value;
}

bool MemberService::isValidEmail(const std::string &email)
{
    // 최소 이메일 형식은 text@text.text 형태다.
    const auto atPos = email.find('@');
    if (atPos == std::string::npos || atPos == 0 || atPos + 1 >= email.size())
    {
        return false;
    }

    const auto dotPos = email.find('.', atPos + 1);
    return dotPos != std::string::npos && dotPos + 1 < email.size();
}

std::string MemberService::randomHex(std::size_t length)
{
    // 허용하는 16진수 문자 목록이다.
    static constexpr std::array<char, 16> kHexChars{
        '0', '1', '2', '3', '4', '5', '6', '7',
        '8', '9', 'a', 'b', 'c', 'd', 'e', 'f'};

    // 16진수 문자를 뽑기 위한 난수 생성기다.
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

std::string MemberService::hashPasswordWithSalt(const std::string &password)
{
    // 저장 형식은 "salt$hash"다.
    const auto salt = randomHex(16);
    const auto hash =
        std::to_string(std::hash<std::string>{}(salt + "::" + password));
    return salt + "$" + hash;
}

bool MemberService::verifyPassword(const std::string &password,
                                   const std::string &storedHash)
{
    const auto separatorPos = storedHash.find('$');
    if (separatorPos == std::string::npos)
    {
        // 과거 평문 저장값과의 임시 호환을 위해 직접 비교한다.
        return storedHash == password;
    }

    // 저장된 솔트로 해시를 다시 계산한다.
    const auto salt = storedHash.substr(0, separatorPos);
    const auto expectedHash = storedHash.substr(separatorPos + 1);
    const auto actualHash =
        std::to_string(std::hash<std::string>{}(salt + "::" + password));
    return expectedHash == actualHash;
}

std::string MemberService::createSession(std::uint64_t memberId)
{
    const auto now = std::chrono::system_clock::now();
    const auto expiresAt = now + std::chrono::seconds(kSessionDurationSeconds);
    const auto sessionToken = randomHex(48);

    // 새 세션을 넣기 전에 만료된 세션을 제거한다.
    std::lock_guard<std::mutex> lock(sessionsMutex_);
    cleanupExpiredSessionsLocked(now);
    sessions_[sessionToken] = SessionData{memberId, expiresAt};

    return sessionToken;
}

std::optional<std::uint64_t> MemberService::resolveSessionMemberId(
    const std::string &sessionToken)
{
    if (sessionToken.empty())
    {
        return std::nullopt;
    }

    const auto now = std::chrono::system_clock::now();

    // sessions_ 조회와 제거는 같은 뮤텍스로 동기화한다.
    std::lock_guard<std::mutex> lock(sessionsMutex_);
    cleanupExpiredSessionsLocked(now);

    const auto it = sessions_.find(sessionToken);
    if (it == sessions_.end())
    {
        return std::nullopt;
    }

    if (it->second.expiresAt <= now)
    {
        // 만료된 토큰은 제거하고 유효하지 않은 것으로 처리한다.
        sessions_.erase(it);
        return std::nullopt;
    }

    return it->second.memberId;
}

void MemberService::cleanupExpiredSessionsLocked(
    const std::chrono::system_clock::time_point &now)
{
    // 제거 작업은 현재 반복자를 무효화하므로 반환된 반복자로 순회를 이어간다.
    for (auto it = sessions_.begin(); it != sessions_.end();)
    {
        if (it->second.expiresAt <= now)
        {
            it = sessions_.erase(it);
            continue;
        }
        ++it;
    }
}

}  // 인증 네임스페이스
