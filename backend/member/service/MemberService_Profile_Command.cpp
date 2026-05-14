/*
 * 파일 개요: 프로필 변경 계열(쓰기) 유스케이스 구현 파일이다.
 * 주요 역할: 프로필 수정, Food MBTI 저장, 입력 정규화/검증, DB 저장 처리.
 */
#include "member/service/MemberService_Profile_Command.h"

#include <drogon/drogon.h>
#include <drogon/orm/Exception.h>

#include <algorithm>
#include <cctype>

#include <json/reader.h>
#include <json/writer.h>

namespace
{

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

// 공백만 있는 입력을 제거하기 위한 보조 함수다.
std::string trimCopy(std::string value)
{
    const auto notSpace = [](unsigned char ch) { return !std::isspace(ch); };
    value.erase(value.begin(), std::find_if(value.begin(), value.end(), notSpace));
    value.erase(std::find_if(value.rbegin(), value.rend(), notSpace).base(), value.end());
    return value;
}

// 문자열 벡터를 JSON 배열 문자열로 직렬화한다.
std::string toJsonArrayString(const std::vector<std::string> &values)
{
    Json::Value jsonArray(Json::arrayValue);
    for (const auto &value : values)
    {
        const auto trimmed = trimCopy(value);
        if (!trimmed.empty())
        {
            jsonArray.append(trimmed);
        }
    }

    Json::StreamWriterBuilder writerBuilder;
    writerBuilder["indentation"] = "";
    return Json::writeString(writerBuilder, jsonArray);
}

// completedAt 문자열을 "YYYY-MM-DD HH:MM:SS" 형식으로 정규화한다.
std::optional<std::string> normalizeCompletedAt(
    const std::optional<std::string> &completedAt)
{
    if (!completedAt.has_value())
    {
        return std::nullopt;
    }

    auto value = trimCopy(*completedAt);
    if (value.empty())
    {
        return std::nullopt;
    }

    if (value.size() == 10U)
    {
        return value + " 00:00:00";
    }

    if (value.size() >= 19U)
    {
        value = value.substr(0, 19);
        if (value[10] == 'T')
        {
            value[10] = ' ';
        }
        if (value[10] == ' ')
        {
            return value;
        }
    }

    return std::nullopt;
}

// 저장 모델을 API 응답 DTO로 변환한다.
auth::FoodMbtiDTO toFoodMbtiDTO(const auth::FoodMbtiModel &model)
{
    auth::FoodMbtiDTO dto;
    dto.type = model.type;
    dto.title = model.title;
    dto.description = model.description;
    dto.completedAt = model.completedAt;
    return dto;
}

}  // namespace

namespace auth
{

std::string MemberService_Profile_Command::trim(std::string value)
{
    // 입력 문자열 양끝 공백을 제거한다.
    const auto notSpace = [](unsigned char ch) { return !std::isspace(ch); };
    value.erase(value.begin(), std::find_if(value.begin(), value.end(), notSpace));
    value.erase(std::find_if(value.rbegin(), value.rend(), notSpace).base(), value.end());
    return value;
}

std::string MemberService_Profile_Command::toLower(std::string value)
{
    // 이메일 비교를 위해 소문자로 정규화한다.
    std::transform(value.begin(), value.end(), value.begin(), [](unsigned char ch) {
        return static_cast<char>(std::tolower(ch));
    });
    return value;
}

bool MemberService_Profile_Command::isValidEmail(const std::string &email)
{
    // 간단한 이메일 형식(@, 도메인 점) 검증을 수행한다.
    const auto atPos = email.find('@');
    if (atPos == std::string::npos || atPos == 0 || atPos + 1 >= email.size())
    {
        return false;
    }

    const auto dotPos = email.find('.', atPos + 1);
    return dotPos != std::string::npos && dotPos + 1 < email.size();
}

UpdateProfileResultDTO MemberService_Profile_Command::updateProfile(
    const std::string &sessionToken,
    const UpdateProfileRequestDTO &request)
{
    // 프로필 수정: 입력 검증 -> 인증 확인 -> 중복검사 -> DB 갱신 순서다.
    UpdateProfileResultDTO result;
    const auto name = trim(request.name);
    const auto email = toLower(trim(request.email));

    if (name.empty() || email.empty())
    {
        result.statusCode = 400;
        result.message = "Please fill in all required fields.";
        return result;
    }

    if (!isValidEmail(email))
    {
        result.statusCode = 400;
        result.message = "Invalid email format.";
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

        const auto currentMember = mapper_.findById(*memberId);
        if (!currentMember.has_value() || currentMember->status != "ACTIVE")
        {
            result.statusCode = 401;
            result.message = "Unauthorized.";
            return result;
        }

        const auto memberWithSameEmail = mapper_.findByEmail(email);
        if (memberWithSameEmail.has_value() &&
            memberWithSameEmail->memberId != *memberId)
        {
            result.statusCode = 409;
            result.message = "Email is already registered.";
            return result;
        }

        mapper_.updateProfile(*memberId, email, name);
        const auto updatedMember = mapper_.findById(*memberId);
        if (!updatedMember.has_value())
        {
            result.statusCode = 500;
            result.message = "Failed to load member.";
            return result;
        }

        result.ok = true;
        result.statusCode = 200;
        result.message = "Profile updated.";
        result.member = mapper_.toMemberDTO(*updatedMember);
        return result;
    }
    catch (const drogon::orm::DrogonDbException &e)
    {
        LOG_ERROR << "update profile db error: " << e.base().what();
        if (looksLikeDuplicateEmail(e.base().what()))
        {
            result.statusCode = 409;
            result.message = "Email is already registered.";
            return result;
        }

        result.statusCode = 500;
        result.message = "Database error while updating profile.";
        return result;
    }
    catch (const std::exception &)
    {
        result.statusCode = 500;
        result.message = "Server error while updating profile.";
        return result;
    }
}

SaveFoodMbtiResultDTO MemberService_Profile_Command::saveMyFoodMbti(
    const std::string &sessionToken,
    const SaveFoodMbtiRequestDTO &request)
{
    // MBTI 저장: 입력 검증 -> 인증 확인 -> UPSERT -> 재조회 순서다.
    SaveFoodMbtiResultDTO result;
    const auto type = trim(request.type);
    const auto title = trim(request.title);
    const auto description = trim(request.description);

    if (type.empty() || title.empty())
    {
        result.statusCode = 400;
        result.message = "Please fill in all required fields.";
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

        mapper_.upsertFoodMbti(*memberId,
                               type,
                               title,
                               description,
                               toJsonArrayString(request.traits),
                               toJsonArrayString(request.recommendedFoods),
                               normalizeCompletedAt(request.completedAt));

        const auto saved = mapper_.findFoodMbtiByMemberId(*memberId);
        if (!saved.has_value())
        {
            result.statusCode = 500;
            result.message = "Failed to load member.";
            return result;
        }

        result.ok = true;
        result.statusCode = 200;
        result.message = "Food MBTI saved.";
        result.foodMbti = toFoodMbtiDTO(*saved);
        return result;
    }
    catch (const drogon::orm::DrogonDbException &)
    {
        result.statusCode = 500;
        result.message = "Database error while updating profile.";
        return result;
    }
    catch (const std::exception &)
    {
        result.statusCode = 500;
        result.message = "Server error while updating profile.";
        return result;
    }
}

}  // namespace auth

