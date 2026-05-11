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

// 로그인/회원가입 성공 시 발급되는 메모리 세션의 유효 시간이다.
constexpr int kSessionDurationSeconds = 60 * 60 * 24;

std::string makeGenericLoginErrorMessage()
{
    // 계정 존재 여부나 상태를 노출하지 않도록 로그인 실패 메시지는 하나로 통일한다.
    return "Invalid email or password.";
}

bool looksLikeDuplicateEmail(const std::string &message)
{
    // DB 드라이버별 duplicate key 메시지가 조금씩 달라서 핵심 단어만 낮은 비용으로 검사한다.
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

AuthResultDTO MemberService::signup(const SignupRequestDTO &request)
{
    AuthResultDTO result;

    // 사용자 입력은 저장/비교 전에 공백과 이메일 대소문자를 정규화한다.
    const auto name = trim(request.name);
    const auto email = toLower(trim(request.email));
    const auto password = request.password;
    const auto confirmPassword = request.confirmPassword;

    // 필수 입력값을 먼저 검사해 DB 접근 전에 빠르게 실패시킨다.
    if (name.empty() || email.empty() || password.empty() || confirmPassword.empty())
    {
        result.statusCode = 400;
        result.message = "Please fill in all required fields.";
        return result;
    }

    // 프론트엔드 약관 체크가 누락된 요청은 회원 생성 전에 차단한다.
    if (!request.agreeTerms)
    {
        result.statusCode = 400;
        result.message = "You must agree to the terms.";
        return result;
    }

    // 현재는 간단한 이메일 형식 검증만 수행한다.
    if (!isValidEmail(email))
    {
        result.statusCode = 400;
        result.message = "Invalid email format.";
        return result;
    }

    // 최소 길이 정책을 서비스 계층에서 강제해 모든 클라이언트에 동일하게 적용한다.
    if (password.size() < 8U)
    {
        result.statusCode = 400;
        result.message = "Password must be at least 8 characters.";
        return result;
    }

    // 확인 비밀번호가 다르면 DB 저장을 시도하지 않는다.
    if (password != confirmPassword)
    {
        result.statusCode = 400;
        result.message = "Passwords do not match.";
        return result;
    }

    try
    {
        // 사용자에게 명확한 409 응답을 주기 위해 INSERT 전에 중복 이메일을 먼저 확인한다.
        if (mapper_.findByEmail(email).has_value())
        {
            result.statusCode = 409;
            result.message = "Email is already registered.";
            return result;
        }

        // 비밀번호 원문은 저장하지 않고 salt가 포함된 hash 문자열만 저장한다.
        const auto passwordHash = hashPasswordWithSalt(password);
        auto createdMember = mapper_.createMember(email, passwordHash, name);

        if (!createdMember.has_value())
        {
            result.statusCode = 500;
            result.message = "Failed to create member.";
            return result;
        }

        // 회원가입 직후 자동 로그인 상태와 맞추기 위해 last_login_at을 갱신한다.
        mapper_.updateLastLoginAt(createdMember->memberId);
        createdMember = mapper_.findById(createdMember->memberId);
        if (!createdMember.has_value())
        {
            result.statusCode = 500;
            result.message = "Failed to load member.";
            return result;
        }

        // 세션 토큰은 응답 쿠키로 내려가고 서버 메모리에 memberId와 함께 보관된다.
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
        // 동시 가입 요청에서는 사전 중복 검사 후에도 DB unique 제약에서 충돌할 수 있다.
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

    // 이메일은 가입 때와 같은 기준으로 비교하기 위해 소문자/trim 처리한다.
    const auto email = toLower(trim(request.email));
    const auto password = request.password;

    // 빈 입력은 인증 실패가 아니라 잘못된 요청으로 처리한다.
    if (email.empty() || password.empty())
    {
        result.statusCode = 400;
        result.message = "Please enter email and password.";
        return result;
    }

    try
    {
        // 이메일로 회원을 찾고, 이후 모든 실패는 동일한 메시지로 숨긴다.
        auto member = mapper_.findByEmail(email);
        if (!member.has_value())
        {
            result.statusCode = 401;
            result.message = makeGenericLoginErrorMessage();
            return result;
        }

        // 비활성 회원은 로그인할 수 없다.
        if (member->status != "ACTIVE")
        {
            result.statusCode = 401;
            result.message = makeGenericLoginErrorMessage();
            return result;
        }

        // 저장된 salt/hash와 입력 비밀번호를 비교한다.
        if (!verifyPassword(password, member->passwordHash))
        {
            result.statusCode = 401;
            result.message = makeGenericLoginErrorMessage();
            return result;
        }

        // 로그인 성공 시각을 저장하고 최신 회원 정보를 다시 읽는다.
        mapper_.updateLastLoginAt(member->memberId);
        member = mapper_.findById(member->memberId);
        if (!member.has_value())
        {
            result.statusCode = 500;
            result.message = "Failed to load member.";
            return result;
        }

        // 이후 요청에서 사용할 세션을 생성한다.
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
        // 쿠키의 세션 토큰을 서버 메모리의 memberId로 변환한다.
        const auto memberId = resolveSessionMemberId(sessionToken);
        if (!memberId.has_value())
        {
            return std::nullopt;
        }

        // 세션이 살아 있어도 회원이 삭제/비활성화됐으면 로그인 상태로 보지 않는다.
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

    // sessions_는 여러 요청이 공유하므로 제거 작업도 mutex 안에서 처리한다.
    std::lock_guard<std::mutex> lock(sessionsMutex_);
    return sessions_.erase(sessionToken) > 0;
}

AwardExperienceResultDTO MemberService::awardExperience(
    const std::string &sessionToken,
    const AwardExperienceRequestDTO &request)
{
    AwardExperienceResultDTO result;

    const auto awardedExp = resolveAwardedExpByActionType(request.actionType);
    if (!awardedExp.has_value())
    {
        result.statusCode = 400;
        result.message = "Invalid action type.";
        return result;
    }

    try
    {
        const auto memberId = resolveSessionMemberId(sessionToken);
        if (!memberId.has_value())
        {
            result.statusCode = 401;
            result.message = "Unauthorized.";
            return result;
        }

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
    const auto notSpace = [](unsigned char ch) { return !std::isspace(ch); };

    value.erase(value.begin(),
                std::find_if(value.begin(), value.end(), notSpace));
    value.erase(
        std::find_if(value.rbegin(), value.rend(), notSpace).base(), value.end());

    return value;
}

std::string MemberService::toLower(std::string value)
{
    std::transform(value.begin(), value.end(), value.begin(), [](unsigned char ch) {
        return static_cast<char>(std::tolower(ch));
    });
    return value;
}

bool MemberService::isValidEmail(const std::string &email)
{
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

std::string MemberService::hashPasswordWithSalt(const std::string &password)
{
    // 저장 형식은 "salt$hash"다. verifyPassword가 같은 구분자로 salt와 hash를 분리한다.
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
        // 과거에 평문으로 저장된 값이 있다면 임시 호환을 위해 직접 비교한다.
        return storedHash == password;
    }

    // 저장된 salt를 재사용해 입력 비밀번호의 hash를 다시 계산한다.
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

    // 새 세션을 넣기 전에 만료된 세션을 같이 정리해 저장소 크기를 제한한다.
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

    // 세션 조회와 만료 삭제가 동시에 일어나지 않도록 같은 mutex로 보호한다.
    std::lock_guard<std::mutex> lock(sessionsMutex_);
    cleanupExpiredSessionsLocked(now);

    const auto it = sessions_.find(sessionToken);
    if (it == sessions_.end())
    {
        return std::nullopt;
    }

    if (it->second.expiresAt <= now)
    {
        // 만료된 토큰은 즉시 제거하고 인증 실패로 처리한다.
        sessions_.erase(it);
        return std::nullopt;
    }

    return it->second.memberId;
}

void MemberService::cleanupExpiredSessionsLocked(
    const std::chrono::system_clock::time_point &now)
{
    // erase가 iterator를 무효화하므로 반환된 iterator로 순회를 이어간다.
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

}  // namespace auth
