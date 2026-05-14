/*
 * 파일 개요: 프로필 조회 계열(읽기) 유스케이스 구현 파일이다.
 * 주요 역할: 세션 사용자 판별, 친구 관계 조회, MBTI JSON 파싱 및 응답 조립.
 */
#include "member/service/MemberService_Profile_Query.h"

#include <algorithm>
#include <cctype>
#include <sstream>

#include <json/reader.h>

namespace
{

// 문자열 양끝 공백을 제거한다.
std::string trimCopy(std::string value)
{
    const auto notSpace = [](unsigned char ch) { return !std::isspace(ch); };
    value.erase(value.begin(), std::find_if(value.begin(), value.end(), notSpace));
    value.erase(std::find_if(value.rbegin(), value.rend(), notSpace).base(), value.end());
    return value;
}

// DB의 JSON 배열 문자열을 vector<string>으로 역직렬화한다.
std::vector<std::string> parseJsonStringArray(const std::string &jsonText)
{
    std::vector<std::string> out;
    if (jsonText.empty())
    {
        return out;
    }

    std::istringstream input(jsonText);
    Json::Value root;
    Json::CharReaderBuilder readerBuilder;
    std::string errors;
    if (!Json::parseFromStream(readerBuilder, input, &root, &errors) ||
        !root.isArray())
    {
        return out;
    }

    out.reserve(root.size());
    for (const auto &item : root)
    {
        if (!item.isString())
        {
            continue;
        }

        const auto value = trimCopy(item.asString());
        if (!value.empty())
        {
            out.push_back(value);
        }
    }

    return out;
}

// FoodMbtiModel -> FoodMbtiDTO 변환
auth::FoodMbtiDTO toFoodMbtiDTO(const auth::FoodMbtiModel &model)
{
    auth::FoodMbtiDTO dto;
    dto.type = model.type;
    dto.title = model.title;
    dto.description = model.description;
    dto.traits = parseJsonStringArray(model.traitsJson);
    dto.recommendedFoods = parseJsonStringArray(model.recommendedFoodsJson);
    dto.completedAt = model.completedAt;
    return dto;
}

// MemberModel + 부가정보 -> MemberProfileDTO 변환
auth::MemberProfileDTO toMemberProfileDTO(
    const auth::MemberModel &member,
    bool isMe,
    const std::optional<auth::FoodMbtiModel> &foodMbti)
{
    auth::MemberProfileDTO profile;
    profile.memberId = member.memberId;
    profile.name = member.name;
    profile.email = member.email;
    profile.level = member.level;
    profile.exp = member.exp;
    profile.isMe = isMe;
    if (foodMbti.has_value())
    {
        profile.foodMbti = toFoodMbtiDTO(*foodMbti);
    }
    return profile;
}

}  // namespace

namespace auth
{

std::optional<MemberDTO> MemberService_Profile_Query::getCurrentMember(
    const std::string &sessionToken)
{
    // 세션 토큰으로 현재 활성 사용자 정보를 조회한다.
    try
    {
        const auto memberId = sessionStore_.resolveSessionMemberId(sessionToken);
        if (!memberId.has_value())
        {
            return std::nullopt;
        }

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

std::optional<std::uint64_t> MemberService_Profile_Query::getCurrentMemberId(
    const std::string &sessionToken)
{
    // 세션 토큰으로 현재 활성 사용자 ID만 조회한다.
    try
    {
        const auto memberId = sessionStore_.resolveSessionMemberId(sessionToken);
        if (!memberId.has_value())
        {
            return std::nullopt;
        }

        const auto member = mapper_.findById(*memberId);
        if (!member.has_value() || member->status != "ACTIVE")
        {
            return std::nullopt;
        }

        return member->memberId;
    }
    catch (const std::exception &)
    {
        return std::nullopt;
    }
}

MemberProfileResultDTO MemberService_Profile_Query::getMyProfile(
    const std::string &sessionToken)
{
    // 내 프로필은 인증된 사용자 기준으로 MBTI까지 함께 조회한다.
    MemberProfileResultDTO result;
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

        const auto foodMbti = mapper_.findFoodMbtiByMemberId(*memberId);
        result.ok = true;
        result.statusCode = 200;
        result.message = "Profile loaded.";
        result.profile = toMemberProfileDTO(*member, true, foodMbti);
        return result;
    }
    catch (const std::exception &)
    {
        result.statusCode = 500;
        result.message = "Failed to load member.";
        return result;
    }
}

MemberProfileResultDTO MemberService_Profile_Query::getMemberProfile(
    const std::string &sessionToken,
    std::uint64_t targetMemberId)
{
    // 타인 프로필은 친구 수락 관계일 때만 공개한다.
    MemberProfileResultDTO result;
    if (targetMemberId == 0)
    {
        result.statusCode = 400;
        result.message = "Invalid member id.";
        return result;
    }

    try
    {
        const auto currentMemberId = sessionStore_.resolveSessionMemberId(sessionToken);
        if (!currentMemberId.has_value())
        {
            result.statusCode = 401;
            result.message = "Unauthorized.";
            return result;
        }

        const auto currentMember = mapper_.findById(*currentMemberId);
        if (!currentMember.has_value() || currentMember->status != "ACTIVE")
        {
            result.statusCode = 401;
            result.message = "Unauthorized.";
            return result;
        }

        const auto targetMember = mapper_.findById(targetMemberId);
        if (!targetMember.has_value() || targetMember->status != "ACTIVE")
        {
            result.statusCode = 404;
            result.message = "Member not found.";
            return result;
        }

        // 본인 프로필이 아니면 친구 수락 관계(ACCEPTED)일 때만 조회를 허용한다.
        const bool isMe = targetMemberId == *currentMemberId;
        if (!isMe)
        {
            const auto relationship =
                friendMapper_.findRelationship(*currentMemberId, targetMemberId);
            if (!relationship.has_value() || relationship->status != "ACCEPTED")
            {
                result.statusCode = 403;
                result.message = "Forbidden.";
                return result;
            }
        }

        const auto foodMbti = mapper_.findFoodMbtiByMemberId(targetMemberId);
        result.ok = true;
        result.statusCode = 200;
        result.message = "Profile loaded.";
        result.profile = toMemberProfileDTO(*targetMember, isMe, foodMbti);
        return result;
    }
    catch (const std::exception &)
    {
        result.statusCode = 500;
        result.message = "Failed to load member.";
        return result;
    }
}

}  // namespace auth

