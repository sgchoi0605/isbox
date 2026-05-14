/*
 * 파일 개요: 검색 문자열/JSON 값 정규화 및 우선순위 계산 유틸 구현 파일이다.
 * 주요 역할: UTF-8 안전 prefix 처리, alias 기반 안전 파싱, 검색 비교 토큰 생성, 정렬 우선순위 계산을 담당한다.
 * 사용 위치: Client/Assembler/검색 서비스 전반에서 텍스트 처리와 데이터 정규화 공통 유틸로 사용된다.
 */
#include "ingredient/service/ProcessedFoodSearchService_Text.h"

#include <algorithm>
#include <cctype>
#include <cstddef>
#include <exception>
#include <regex>
#include <unordered_set>
#include <vector>

namespace
{

// UTF-8 선두 바이트를 해석해 현재 코드포인트 길이를 계산한다.
std::size_t readUtf8CodePointByteLength(const std::string &text, std::size_t offset)
{
    if (offset >= text.size())
    {
        return 0U;
    }

    const auto lead = static_cast<unsigned char>(text[offset]);
    std::size_t byteLength = 1U;
    if ((lead & 0x80U) == 0U)
    {
        byteLength = 1U;
    }
    else if ((lead & 0xE0U) == 0xC0U)
    {
        byteLength = 2U;
    }
    else if ((lead & 0xF0U) == 0xE0U)
    {
        byteLength = 3U;
    }
    else if ((lead & 0xF8U) == 0xF0U)
    {
        byteLength = 4U;
    }
    else
    {
        return 1U;
    }

    if (offset + byteLength > text.size())
    {
        return 1U;
    }

    for (std::size_t index = offset + 1U; index < offset + byteLength; ++index)
    {
        const auto byte = static_cast<unsigned char>(text[index]);
        if ((byte & 0xC0U) != 0x80U)
        {
            return 1U;
        }
    }

    return byteLength;
}

// 코드포인트 경계 인덱스를 누적해 prefix 잘라내기에 사용한다.
std::vector<std::size_t> collectUtf8CodePointEnds(const std::string &text)
{
    std::vector<std::size_t> codePointEnds;
    codePointEnds.reserve(text.size() + 1U);
    codePointEnds.push_back(0U);

    std::size_t offset = 0U;
    while (offset < text.size())
    {
        const auto byteLength = readUtf8CodePointByteLength(text, offset);
        if (byteLength == 0U)
        {
            break;
        }
        offset += byteLength;
        codePointEnds.push_back(offset);
    }

    return codePointEnds;
}

}  // namespace

namespace ingredient
{

// 공백 차이로 인한 검색/비교 오차를 줄이기 위해 trim 처리한다.
std::string ProcessedFoodSearchService_Text::trim(std::string value)
{
    const auto notSpace = [](unsigned char ch) { return !std::isspace(ch); };
    value.erase(value.begin(), std::find_if(value.begin(), value.end(), notSpace));
    value.erase(std::find_if(value.rbegin(), value.rend(), notSpace).base(), value.end());
    return value;
}

// 영문 대소문자 차이를 제거한다.
std::string ProcessedFoodSearchService_Text::toLowerAscii(std::string value)
{
    std::transform(value.begin(), value.end(), value.begin(), [](unsigned char ch) {
        return static_cast<char>(std::tolower(ch));
    });
    return value;
}

// 검색 비교에 쓰는 정규화 키를 생성한다.
std::string ProcessedFoodSearchService_Text::normalizeForSearch(std::string value)
{
    value = toLowerAscii(trim(std::move(value)));
    value.erase(std::remove_if(value.begin(),
                               value.end(),
                               [](unsigned char ch) {
                                   return std::isspace(ch) != 0;
                               }),
                value.end());
    return value;
}

// 이미 인코딩된 값을 이중 인코딩하지 않기 위한 체크.
bool ProcessedFoodSearchService_Text::looksLikePercentEncoded(const std::string &value)
{
    for (std::size_t i = 0; i + 2 < value.size(); ++i)
    {
        if (value[i] != '%')
        {
            continue;
        }

        const auto h1 = static_cast<unsigned char>(value[i + 1]);
        const auto h2 = static_cast<unsigned char>(value[i + 2]);
        if (std::isxdigit(h1) != 0 && std::isxdigit(h2) != 0)
        {
            return true;
        }
    }
    return false;
}

// null/빈값을 제외하고 문자열 값을 안전하게 꺼낸다.
std::optional<std::string> ProcessedFoodSearchService_Text::optionalJsonString(
    const Json::Value &value)
{
    if (value.isNull())
    {
        return std::nullopt;
    }

    if (value.isString())
    {
        const auto str = value.asString();
        if (str.empty())
        {
            return std::nullopt;
        }
        return str;
    }

    if (value.isNumeric())
    {
        return value.asString();
    }

    return std::nullopt;
}

// key alias 목록에서 첫 유효 문자열 값을 반환한다.
std::optional<std::string> ProcessedFoodSearchService_Text::firstStringFromKeys(
    const Json::Value &row,
    std::initializer_list<const char *> keys)
{
    for (const auto *key : keys)
    {
        if (!row.isMember(key))
        {
            continue;
        }

        const auto value = optionalJsonString(row[key]);
        if (value.has_value() && !value->empty())
        {
            return value;
        }
    }

    return std::nullopt;
}

// 숫자/문자열 형태를 모두 허용해 double로 변환한다.
std::optional<double> ProcessedFoodSearchService_Text::optionalJsonDouble(
    const Json::Value &value)
{
    if (value.isNull())
    {
        return std::nullopt;
    }

    try
    {
        if (value.isNumeric())
        {
            return value.asDouble();
        }

        if (value.isString())
        {
            auto text = value.asString();
            text.erase(
                std::remove_if(text.begin(), text.end(), [](unsigned char ch) {
                    return std::isspace(ch) != 0 || ch == ',';
                }),
                text.end());

            if (text.empty())
            {
                return std::nullopt;
            }

            return std::stod(text);
        }
    }
    catch (const std::exception &)
    {
        return std::nullopt;
    }

    return std::nullopt;
}

// key alias 목록에서 첫 유효 숫자값을 반환한다.
std::optional<double> ProcessedFoodSearchService_Text::firstDoubleFromKeys(
    const Json::Value &row,
    std::initializer_list<const char *> keys)
{
    for (const auto *key : keys)
    {
        if (!row.isMember(key))
        {
            continue;
        }

        const auto value = optionalJsonDouble(row[key]);
        if (value.has_value())
        {
            return value;
        }
    }

    return std::nullopt;
}

// 양수 정수 필드를 안전하게 uint64로 변환한다.
std::optional<std::uint64_t> ProcessedFoodSearchService_Text::optionalJsonUInt64(
    const Json::Value &value)
{
    if (value.isNull())
    {
        return std::nullopt;
    }

    try
    {
        if (value.isUInt64())
        {
            return value.asUInt64();
        }
        if (value.isUInt())
        {
            return static_cast<std::uint64_t>(value.asUInt());
        }
        if (value.isNumeric())
        {
            const auto raw = value.asDouble();
            if (raw < 0.0)
            {
                return std::nullopt;
            }
            return static_cast<std::uint64_t>(raw);
        }
        if (value.isString())
        {
            auto text = value.asString();
            text.erase(
                std::remove_if(text.begin(), text.end(), [](unsigned char ch) {
                    return std::isspace(ch) != 0 || ch == ',';
                }),
                text.end());
            if (text.empty())
            {
                return std::nullopt;
            }
            return static_cast<std::uint64_t>(std::stoull(text));
        }
    }
    catch (const std::exception &)
    {
        return std::nullopt;
    }

    return std::nullopt;
}

// importYn은 Y/N 외 입력을 null로 정리한다.
std::optional<std::string> ProcessedFoodSearchService_Text::normalizeImportYnForApi(
    const std::optional<std::string> &value)
{
    if (!value.has_value())
    {
        return std::nullopt;
    }

    const auto normalized = toLowerAscii(trim(*value));
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

// 기준일자 문자열을 공백 제거 후 표준 날짜 표기로 맞춘다.
std::optional<std::string> ProcessedFoodSearchService_Text::normalizeDataBaseDate(
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

// normalize된 문자열에 token이 포함되는지 검사한다.
bool ProcessedFoodSearchService_Text::containsNormalizedToken(const std::string &text,
                                                              const std::string &token)
{
    if (token.empty())
    {
        return true;
    }

    auto normalizedText = normalizeForSearch(text);
    return normalizedText.find(token) != std::string::npos;
}

// UTF-8 코드포인트 기준으로 prefix 토큰들을 생성한다.
std::vector<std::string> ProcessedFoodSearchService_Text::buildUtf8PrefixTokens(
    const std::string &keyword,
    std::size_t minCodePoints,
    std::size_t maxTokenCount)
{
    std::vector<std::string> tokens;
    if (keyword.empty() || maxTokenCount == 0U)
    {
        return tokens;
    }

    const auto codePointEnds = collectUtf8CodePointEnds(keyword);
    if (codePointEnds.size() < 2U)
    {
        return tokens;
    }

    const auto codePointCount = codePointEnds.size() - 1U;
    if (codePointCount <= minCodePoints)
    {
        return tokens;
    }

    std::unordered_set<std::string> seen;
    seen.reserve(maxTokenCount + 1U);

    for (std::size_t count = codePointCount; count > minCodePoints; --count)
    {
        const auto prefixLength = codePointEnds[count - 1U];
        if (prefixLength == 0U || prefixLength > keyword.size())
        {
            continue;
        }

        auto token = keyword.substr(0U, prefixLength);
        if (token.empty())
        {
            continue;
        }
        if (!seen.insert(token).second)
        {
            continue;
        }

        tokens.push_back(std::move(token));
        if (tokens.size() >= maxTokenCount)
        {
            break;
        }
    }

    return tokens;
}

// 검색 정렬 우선순위: exact(0) -> prefix(1) -> partial(2).
int ProcessedFoodSearchService_Text::searchPriority(
    const ProcessedFoodSearchItemDTO &food,
    const std::string &normalizedKeywordToken)
{
    if (normalizedKeywordToken.empty())
    {
        return 2;
    }

    auto normalizedFoodName = normalizeForSearch(food.foodName);
    if (normalizedFoodName == normalizedKeywordToken)
    {
        return 0;
    }
    if (normalizedFoodName.rfind(normalizedKeywordToken, 0) == 0)
    {
        return 1;
    }
    return 2;
}

}  // namespace ingredient


