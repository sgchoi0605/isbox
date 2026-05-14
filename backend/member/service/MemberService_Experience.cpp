/*
 * 파일 개요: 사용자 행동 기반 경험치 지급 규칙 구현 파일이다.
 * 주요 역할: 액션 타입 정규화, 지급 EXP 계산, 레벨/경험치 업데이트 처리.
 */
#include "member/service/MemberService_Experience.h"

#include <algorithm>
#include <cctype>

namespace auth
{

std::string MemberService_Experience::trim(std::string value)
{
    // 행동 타입 앞뒤 공백을 제거한다.
    const auto notSpace = [](unsigned char ch) { return !std::isspace(ch); };
    value.erase(value.begin(), std::find_if(value.begin(), value.end(), notSpace));
    value.erase(std::find_if(value.rbegin(), value.rend(), notSpace).base(), value.end());
    return value;
}

std::string MemberService_Experience::toLower(std::string value)
{
    // 행동 타입 비교를 소문자 기준으로 맞춘다.
    std::transform(value.begin(), value.end(), value.begin(), [](unsigned char ch) {
        return static_cast<char>(std::tolower(ch));
    });
    return value;
}

std::optional<unsigned int> MemberService_Experience::resolveAwardedExpByActionType(
    const std::string &actionType)
{
    // 액션 타입별 보상 EXP 규칙 테이블이다.
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

AwardExperienceResultDTO MemberService_Experience::awardExperience(
    const std::string &sessionToken,
    const AwardExperienceRequestDTO &request)
{
    // 경험치 지급: 액션 검증 -> 인증 확인 -> 레벨 계산 -> DB 반영 순서다.
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
        const auto memberId = sessionStore_.resolveSessionMemberId(sessionToken);
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

        // 레벨업 규칙: 현재 레벨 * 100 EXP를 소모하면 다음 레벨로 상승한다.
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

}  // namespace auth

