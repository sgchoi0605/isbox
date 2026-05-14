/*
 * 파일 개요: 식재료 서비스 공통 검증/정규화 유틸 구현 파일이다.
 * 주요 역할: 문자열 정리, 날짜/enum 검증, DB-클라이언트 필드 포맷 변환 규칙을 일관되게 제공한다.
 * 사용 위치: IngredientService_Query/Command 및 mapper 응답 정규화 단계에서 공통 규칙 모듈로 호출된다.
 */
#include "ingredient/service/IngredientService_Validation.h"

#include <algorithm>
#include <cctype>
#include <regex>

namespace ingredient
{

// 비교/검증 기준을 통일하기 위해 양끝 공백을 제거한다.
std::string IngredientService_Validation::trim(std::string value)
{
    const auto notSpace = [](unsigned char ch) { return !std::isspace(ch); };
    value.erase(value.begin(), std::find_if(value.begin(), value.end(), notSpace));
    value.erase(std::find_if(value.rbegin(), value.rend(), notSpace).base(), value.end());
    return value;
}

// 코드값 비교를 위해 영문 대문자를 소문자로 변환한다.
std::string IngredientService_Validation::toLower(std::string value)
{
    std::transform(value.begin(), value.end(), value.begin(), [](unsigned char ch) {
        return static_cast<char>(std::tolower(ch));
    });
    return value;
}

// 날짜는 YYYY-MM-DD 형식만 허용한다.
bool IngredientService_Validation::isValidDate(const std::string &value)
{
    static const std::regex pattern("^[0-9]{4}-[0-9]{2}-[0-9]{2}$");
    return std::regex_match(value, pattern);
}

// API가 허용하는 storage enum만 통과시킨다.
bool IngredientService_Validation::isAllowedClientStorage(const std::string &value)
{
    return value == "fridge" || value == "freezer" || value == "roomtemp" ||
           value == "other";
}

// 클라이언트 storage 값을 DB ENUM 표현으로 변환한다.
std::string IngredientService_Validation::toDbStorage(const std::string &clientValue)
{
    if (clientValue == "fridge")
    {
        return "FRIDGE";
    }
    if (clientValue == "freezer")
    {
        return "FREEZER";
    }
    if (clientValue == "roomtemp")
    {
        return "ROOM_TEMP";
    }
    return "OTHER";
}

// DB ENUM 값을 클라이언트 응답 표현으로 되돌린다.
std::string IngredientService_Validation::toClientStorage(const std::string &dbValue)
{
    const auto normalized = toLower(trim(dbValue));
    if (normalized == "fridge")
    {
        return "fridge";
    }
    if (normalized == "freezer")
    {
        return "freezer";
    }
    if (normalized == "room_temp")
    {
        return "roomTemp";
    }
    return "other";
}

// importYn은 Y/N 외 값을 null로 정리해 데이터 품질을 보수적으로 유지한다.
std::optional<std::string> IngredientService_Validation::normalizeImportYn(
    const std::optional<std::string> &value)
{
    if (!value.has_value())
    {
        return std::nullopt;
    }

    const auto normalized = toLower(trim(*value));
    if (normalized == "y")
    {
        return "Y";
    }
    if (normalized == "n")
    {
        return "N";
    }
    return std::nullopt;
}

// 기준일자는 공백 제거 후 8자리 숫자면 하이픈 포맷으로 보정한다.
std::optional<std::string> IngredientService_Validation::normalizeDataBaseDate(
    const std::optional<std::string> &value)
{
    if (!value.has_value())
    {
        return std::nullopt;
    }

    std::string text = *value;
    text.erase(
        std::remove_if(text.begin(), text.end(), [](unsigned char ch) {
            return std::isspace(ch) != 0;
        }),
        text.end());

    if (text.empty())
    {
        return std::nullopt;
    }

    static const std::regex yyyymmdd("^[0-9]{8}$");
    if (std::regex_match(text, yyyymmdd))
    {
        return text.substr(0, 4) + "-" + text.substr(4, 2) + "-" +
               text.substr(6, 2);
    }

    return text;
}

// 조회 응답 직전에 저장 포맷을 클라이언트 포맷으로 일괄 정규화한다.
void IngredientService_Validation::normalizeIngredientForClient(IngredientDTO &ingredient)
{
    ingredient.storage = toClientStorage(ingredient.storage);
    ingredient.importYn = normalizeImportYn(ingredient.importYn);
    ingredient.dataBaseDate = normalizeDataBaseDate(ingredient.dataBaseDate);
}

}  // namespace ingredient


