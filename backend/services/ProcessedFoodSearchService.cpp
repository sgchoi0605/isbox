// 이 구현 파일에서 사용하는 의존 헤더를 포함한다.
#include "ProcessedFoodSearchService.h"

// 이 구현 파일에서 사용하는 의존 헤더를 포함한다.
#include <drogon/drogon.h>
// 이 구현 파일에서 사용하는 의존 헤더를 포함한다.
#include <drogon/utils/Utilities.h>

// 이 구현 파일에서 사용하는 의존 헤더를 포함한다.
#include <algorithm>
// 이 구현 파일에서 사용하는 의존 헤더를 포함한다.
#include <cctype>
// 이 구현 파일에서 사용하는 의존 헤더를 포함한다.
#include <chrono>
// 이 구현 파일에서 사용하는 의존 헤더를 포함한다.
#include <cstdlib>
// 이 구현 파일에서 사용하는 의존 헤더를 포함한다.
#include <exception>
// 이 구현 파일에서 사용하는 의존 헤더를 포함한다.
#include <filesystem>
// 이 구현 파일에서 사용하는 의존 헤더를 포함한다.
#include <fstream>
// 이 구현 파일에서 사용하는 의존 헤더를 포함한다.
#include <mutex>
// 이 구현 파일에서 사용하는 의존 헤더를 포함한다.
#include <regex>
// 이 구현 파일에서 사용하는 의존 헤더를 포함한다.
#include <thread>
// 이 구현 파일에서 사용하는 의존 헤더를 포함한다.
#include <unordered_map>
// 이 구현 파일에서 사용하는 의존 헤더를 포함한다.
#include <unordered_set>
// 이 구현 파일에서 사용하는 의존 헤더를 포함한다.
#include <utility>
// 이 구현 파일에서 사용하는 의존 헤더를 포함한다.
#include <vector>

// 심볼 가시성 제어를 위해 네임스페이스 범위를 연다.
namespace
// 새 코드 블록 범위를 시작한다.
{

// 가공식품 검색 흐름에서 사용하는 함수를 선언하거나 정의한다.
std::optional<std::string> optionalJsonString(const Json::Value &value)
// 새 코드 블록 범위를 시작한다.
{
    // 조건을 검사하고 만족 시 분기한다.
    if (value.isNull())
    // 새 코드 블록 범위를 시작한다.
    {
        // 현재 함수에서 값을 반환한다.
        return std::nullopt;
    // 현재 코드 블록 범위를 종료한다.
    }

    // 조건을 검사하고 만족 시 분기한다.
    if (value.isString())
    // 새 코드 블록 범위를 시작한다.
    {
        // 이후 로직에서 사용할 값을 계산하거나 대입한다.
        const auto str = value.asString();
        // 조건을 검사하고 만족 시 분기한다.
        if (str.empty())
        // 새 코드 블록 범위를 시작한다.
        {
            // 현재 함수에서 값을 반환한다.
            return std::nullopt;
        // 현재 코드 블록 범위를 종료한다.
        }
        // 현재 함수에서 값을 반환한다.
        return str;
    // 현재 코드 블록 범위를 종료한다.
    }

    // 조건을 검사하고 만족 시 분기한다.
    if (value.isNumeric())
    // 새 코드 블록 범위를 시작한다.
    {
        // 현재 함수에서 값을 반환한다.
        return value.asString();
    // 현재 코드 블록 범위를 종료한다.
    }

    // 현재 함수에서 값을 반환한다.
    return std::nullopt;
// 현재 코드 블록 범위를 종료한다.
}

// 가공식품 검색 흐름에서 사용하는 함수를 선언하거나 정의한다.
std::optional<std::string> firstStringFromKeys(
    // 검색 워크플로우의 다음 구문을 실행한다.
    const Json::Value &row,
    // 데이터 변환/관리를 위해 표준 라이브러리 유틸리티를 사용한다.
    std::initializer_list<const char *> keys)
// 새 코드 블록 범위를 시작한다.
{
    // 컬렉션 또는 수치 범위를 순회한다.
    for (const auto *key : keys)
    // 새 코드 블록 범위를 시작한다.
    {
        // 조건을 검사하고 만족 시 분기한다.
        if (!row.isMember(key))
        // 새 코드 블록 범위를 시작한다.
        {
            // 이번 반복의 나머지를 건너뛰고 다음 반복으로 이동한다.
            continue;
        // 현재 코드 블록 범위를 종료한다.
        }

        // 이후 로직에서 사용할 값을 계산하거나 대입한다.
        const auto value = optionalJsonString(row[key]);
        // 조건을 검사하고 만족 시 분기한다.
        if (value.has_value() && !value->empty())
        // 새 코드 블록 범위를 시작한다.
        {
            // 현재 함수에서 값을 반환한다.
            return value;
        // 현재 코드 블록 범위를 종료한다.
        }
    // 현재 코드 블록 범위를 종료한다.
    }

    // 현재 함수에서 값을 반환한다.
    return std::nullopt;
// 현재 코드 블록 범위를 종료한다.
}

// 가공식품 검색 흐름에서 사용하는 함수를 선언하거나 정의한다.
std::optional<double> optionalJsonDouble(const Json::Value &value)
// 새 코드 블록 범위를 시작한다.
{
    // 조건을 검사하고 만족 시 분기한다.
    if (value.isNull())
    // 새 코드 블록 범위를 시작한다.
    {
        // 현재 함수에서 값을 반환한다.
        return std::nullopt;
    // 현재 코드 블록 범위를 종료한다.
    }

    // 예외가 발생할 수 있는 블록을 시작하고 예외 처리로 보호한다.
    try
    // 새 코드 블록 범위를 시작한다.
    {
        // 조건을 검사하고 만족 시 분기한다.
        if (value.isNumeric())
        // 새 코드 블록 범위를 시작한다.
        {
            // 현재 함수에서 값을 반환한다.
            return value.asDouble();
        // 현재 코드 블록 범위를 종료한다.
        }

        // 조건을 검사하고 만족 시 분기한다.
        if (value.isString())
        // 새 코드 블록 범위를 시작한다.
        {
            // 이후 로직에서 사용할 값을 계산하거나 대입한다.
            auto text = value.asString();
            // 검색 워크플로우의 다음 구문을 실행한다.
            text.erase(
                // 가공식품 검색 흐름에서 사용하는 함수를 선언하거나 정의한다.
                std::remove_if(text.begin(), text.end(), [](unsigned char ch) {
                    // 현재 함수에서 값을 반환한다.
                    return std::isspace(ch) != 0 || ch == ',';
                // 검색 워크플로우의 다음 구문을 실행한다.
                }),
                // 검색 워크플로우의 다음 구문을 실행한다.
                text.end());

            // 조건을 검사하고 만족 시 분기한다.
            if (text.empty())
            // 새 코드 블록 범위를 시작한다.
            {
                // 현재 함수에서 값을 반환한다.
                return std::nullopt;
            // 현재 코드 블록 범위를 종료한다.
            }

            // 현재 함수에서 값을 반환한다.
            return std::stod(text);
        // 현재 코드 블록 범위를 종료한다.
        }
    // 현재 코드 블록 범위를 종료한다.
    }
    // 보호된 블록에서 발생한 예외를 처리한다.
    catch (const std::exception &)
    // 새 코드 블록 범위를 시작한다.
    {
        // 현재 함수에서 값을 반환한다.
        return std::nullopt;
    // 현재 코드 블록 범위를 종료한다.
    }

    // 현재 함수에서 값을 반환한다.
    return std::nullopt;
// 현재 코드 블록 범위를 종료한다.
}

// 가공식품 검색 흐름에서 사용하는 함수를 선언하거나 정의한다.
std::optional<double> firstDoubleFromKeys(const Json::Value &row,
                                          // 데이터 변환/관리를 위해 표준 라이브러리 유틸리티를 사용한다.
                                          std::initializer_list<const char *> keys)
// 새 코드 블록 범위를 시작한다.
{
    // 컬렉션 또는 수치 범위를 순회한다.
    for (const auto *key : keys)
    // 새 코드 블록 범위를 시작한다.
    {
        // 조건을 검사하고 만족 시 분기한다.
        if (!row.isMember(key))
        // 새 코드 블록 범위를 시작한다.
        {
            // 이번 반복의 나머지를 건너뛰고 다음 반복으로 이동한다.
            continue;
        // 현재 코드 블록 범위를 종료한다.
        }

        // 이후 로직에서 사용할 값을 계산하거나 대입한다.
        const auto value = optionalJsonDouble(row[key]);
        // 조건을 검사하고 만족 시 분기한다.
        if (value.has_value())
        // 새 코드 블록 범위를 시작한다.
        {
            // 현재 함수에서 값을 반환한다.
            return value;
        // 현재 코드 블록 범위를 종료한다.
        }
    // 현재 코드 블록 범위를 종료한다.
    }

    // 현재 함수에서 값을 반환한다.
    return std::nullopt;
// 현재 코드 블록 범위를 종료한다.
}

// 가공식품 검색 흐름에서 사용하는 함수를 선언하거나 정의한다.
std::string toLowerAscii(std::string value)
// 새 코드 블록 범위를 시작한다.
{
    // 가공식품 검색 흐름에서 사용하는 함수를 선언하거나 정의한다.
    std::transform(value.begin(), value.end(), value.begin(), [](unsigned char ch) {
        // 현재 함수에서 값을 반환한다.
        return static_cast<char>(std::tolower(ch));
    // 검색 워크플로우의 다음 구문을 실행한다.
    });
    // 현재 함수에서 값을 반환한다.
    return value;
// 현재 코드 블록 범위를 종료한다.
}

// 가공식품 검색 흐름에서 사용하는 함수를 선언하거나 정의한다.
std::string trimCopy(std::string value)
// 새 코드 블록 범위를 시작한다.
{
    // 가공식품 검색 흐름에서 사용하는 함수를 선언하거나 정의한다.
    const auto notSpace = [](unsigned char ch) { return !std::isspace(ch); };
    // 검색 워크플로우의 다음 구문을 실행한다.
    value.erase(value.begin(),
                // 가공식품 검색 흐름에서 사용하는 함수를 선언하거나 정의한다.
                std::find_if(value.begin(), value.end(), notSpace));
    // 검색 워크플로우의 다음 구문을 실행한다.
    value.erase(
        // 가공식품 검색 흐름에서 사용하는 함수를 선언하거나 정의한다.
        std::find_if(value.rbegin(), value.rend(), notSpace).base(), value.end());
    // 현재 함수에서 값을 반환한다.
    return value;
// 현재 코드 블록 범위를 종료한다.
}

// 가공식품 검색 흐름에서 사용하는 함수를 선언하거나 정의한다.
std::optional<std::string> normalizeImportYnForApi(
    // 검색 워크플로우의 다음 구문을 실행한다.
    const std::optional<std::string> &value)
// 새 코드 블록 범위를 시작한다.
{
    // 조건을 검사하고 만족 시 분기한다.
    if (!value.has_value())
    // 새 코드 블록 범위를 시작한다.
    {
        // 현재 함수에서 값을 반환한다.
        return std::nullopt;
    // 현재 코드 블록 범위를 종료한다.
    }

    // 이후 로직에서 사용할 값을 계산하거나 대입한다.
    const auto normalized = toLowerAscii(trimCopy(*value));
    // 조건을 검사하고 만족 시 분기한다.
    if (normalized == "y")
    // 새 코드 블록 범위를 시작한다.
    {
        // 현재 함수에서 값을 반환한다.
        return "Y";
    // 현재 코드 블록 범위를 종료한다.
    }
    // 조건을 검사하고 만족 시 분기한다.
    if (normalized == "n")
    // 새 코드 블록 범위를 시작한다.
    {
        // 현재 함수에서 값을 반환한다.
        return "N";
    // 현재 코드 블록 범위를 종료한다.
    }
    // 현재 함수에서 값을 반환한다.
    return std::nullopt;
// 현재 코드 블록 범위를 종료한다.
}

// 가공식품 검색 흐름에서 사용하는 함수를 선언하거나 정의한다.
std::optional<std::string> normalizeDataBaseDate(
    // 검색 워크플로우의 다음 구문을 실행한다.
    const std::optional<std::string> &value)
// 새 코드 블록 범위를 시작한다.
{
    // 조건을 검사하고 만족 시 분기한다.
    if (!value.has_value())
    // 새 코드 블록 범위를 시작한다.
    {
        // 현재 함수에서 값을 반환한다.
        return std::nullopt;
    // 현재 코드 블록 범위를 종료한다.
    }

    // 이후 로직에서 사용할 값을 계산하거나 대입한다.
    std::string text = *value;
    // 검색 워크플로우의 다음 구문을 실행한다.
    text.erase(
        // 가공식품 검색 흐름에서 사용하는 함수를 선언하거나 정의한다.
        std::remove_if(text.begin(), text.end(), [](unsigned char ch) {
            // 현재 함수에서 값을 반환한다.
            return std::isspace(ch) != 0;
        // 검색 워크플로우의 다음 구문을 실행한다.
        }),
        // 검색 워크플로우의 다음 구문을 실행한다.
        text.end());

    // 조건을 검사하고 만족 시 분기한다.
    if (text.empty())
    // 새 코드 블록 범위를 시작한다.
    {
        // 현재 함수에서 값을 반환한다.
        return std::nullopt;
    // 현재 코드 블록 범위를 종료한다.
    }

    // 여러 함수 호출 간 재사용할 정적 저장소를 정의한다.
    static const std::regex yyyymmdd("^[0-9]{8}$");
    // 조건을 검사하고 만족 시 분기한다.
    if (std::regex_match(text, yyyymmdd))
    // 새 코드 블록 범위를 시작한다.
    {
        // 현재 함수에서 값을 반환한다.
        return text.substr(0, 4) + "-" + text.substr(4, 2) + "-" +
               // 검색 워크플로우의 다음 구문을 실행한다.
               text.substr(6, 2);
    // 현재 코드 블록 범위를 종료한다.
    }

    // 현재 함수에서 값을 반환한다.
    return text;
// 현재 코드 블록 범위를 종료한다.
}

// 가공식품 검색 흐름에서 사용하는 함수를 선언하거나 정의한다.
std::optional<std::uint64_t> optionalJsonUInt64(const Json::Value &value)
// 새 코드 블록 범위를 시작한다.
{
    // 조건을 검사하고 만족 시 분기한다.
    if (value.isNull())
    // 새 코드 블록 범위를 시작한다.
    {
        // 현재 함수에서 값을 반환한다.
        return std::nullopt;
    // 현재 코드 블록 범위를 종료한다.
    }

    // 예외가 발생할 수 있는 블록을 시작하고 예외 처리로 보호한다.
    try
    // 새 코드 블록 범위를 시작한다.
    {
        // 조건을 검사하고 만족 시 분기한다.
        if (value.isUInt64())
        // 새 코드 블록 범위를 시작한다.
        {
            // 현재 함수에서 값을 반환한다.
            return value.asUInt64();
        // 현재 코드 블록 범위를 종료한다.
        }
        // 조건을 검사하고 만족 시 분기한다.
        if (value.isUInt())
        // 새 코드 블록 범위를 시작한다.
        {
            // 현재 함수에서 값을 반환한다.
            return static_cast<std::uint64_t>(value.asUInt());
        // 현재 코드 블록 범위를 종료한다.
        }
        // 조건을 검사하고 만족 시 분기한다.
        if (value.isNumeric())
        // 새 코드 블록 범위를 시작한다.
        {
            // 이후 로직에서 사용할 값을 계산하거나 대입한다.
            const auto raw = value.asDouble();
            // 조건을 검사하고 만족 시 분기한다.
            if (raw < 0.0)
            // 새 코드 블록 범위를 시작한다.
            {
                // 현재 함수에서 값을 반환한다.
                return std::nullopt;
            // 현재 코드 블록 범위를 종료한다.
            }
            // 현재 함수에서 값을 반환한다.
            return static_cast<std::uint64_t>(raw);
        // 현재 코드 블록 범위를 종료한다.
        }
        // 조건을 검사하고 만족 시 분기한다.
        if (value.isString())
        // 새 코드 블록 범위를 시작한다.
        {
            // 이후 로직에서 사용할 값을 계산하거나 대입한다.
            auto text = value.asString();
            // 검색 워크플로우의 다음 구문을 실행한다.
            text.erase(
                // 가공식품 검색 흐름에서 사용하는 함수를 선언하거나 정의한다.
                std::remove_if(text.begin(), text.end(), [](unsigned char ch) {
                    // 현재 함수에서 값을 반환한다.
                    return std::isspace(ch) != 0 || ch == ',';
                // 검색 워크플로우의 다음 구문을 실행한다.
                }),
                // 검색 워크플로우의 다음 구문을 실행한다.
                text.end());
            // 조건을 검사하고 만족 시 분기한다.
            if (text.empty())
            // 새 코드 블록 범위를 시작한다.
            {
                // 현재 함수에서 값을 반환한다.
                return std::nullopt;
            // 현재 코드 블록 범위를 종료한다.
            }
            // 현재 함수에서 값을 반환한다.
            return static_cast<std::uint64_t>(std::stoull(text));
        // 현재 코드 블록 범위를 종료한다.
        }
    // 현재 코드 블록 범위를 종료한다.
    }
    // 보호된 블록에서 발생한 예외를 처리한다.
    catch (const std::exception &)
    // 새 코드 블록 범위를 시작한다.
    {
        // 현재 함수에서 값을 반환한다.
        return std::nullopt;
    // 현재 코드 블록 범위를 종료한다.
    }

    // 현재 함수에서 값을 반환한다.
    return std::nullopt;
// 현재 코드 블록 범위를 종료한다.
}

// 가공식품 검색 흐름에서 사용하는 함수를 선언하거나 정의한다.
std::optional<std::string> extractPublicApiErrorMessage(const Json::Value &body)
// 새 코드 블록 범위를 시작한다.
{
    // 조건을 검사하고 만족 시 분기한다.
    if (!body.isMember("response") || !body["response"].isObject())
    // 새 코드 블록 범위를 시작한다.
    {
        // 현재 함수에서 값을 반환한다.
        return std::nullopt;
    // 현재 코드 블록 범위를 종료한다.
    }

    // 이후 로직에서 사용할 값을 계산하거나 대입한다.
    const auto &responseRoot = body["response"];
    // 조건을 검사하고 만족 시 분기한다.
    if (!responseRoot.isMember("header") || !responseRoot["header"].isObject())
    // 새 코드 블록 범위를 시작한다.
    {
        // 현재 함수에서 값을 반환한다.
        return std::nullopt;
    // 현재 코드 블록 범위를 종료한다.
    }

    // 이후 로직에서 사용할 값을 계산하거나 대입한다.
    const auto &header = responseRoot["header"];
    // 이후 로직에서 사용할 값을 계산하거나 대입한다.
    const auto resultCode =
        // 가공식품 검색 흐름에서 사용하는 함수를 선언하거나 정의한다.
        firstStringFromKeys(header, {"resultCode", "RESULT_CODE"});
    // 이후 로직에서 사용할 값을 계산하거나 대입한다.
    const auto resultMsg = firstStringFromKeys(header, {"resultMsg", "RESULT_MSG"});
    // 조건을 검사하고 만족 시 분기한다.
    if (!resultCode.has_value() || *resultCode == "00")
    // 새 코드 블록 범위를 시작한다.
    {
        // 현재 함수에서 값을 반환한다.
        return std::nullopt;
    // 현재 코드 블록 범위를 종료한다.
    }

    // 현재 함수에서 값을 반환한다.
    return "Nutrition public API error (" + *resultCode +
           // 검색 워크플로우의 다음 구문을 실행한다.
           "): " + resultMsg.value_or("Unknown error.");
// 현재 코드 블록 범위를 종료한다.
}

// 가공식품 검색 흐름에서 사용하는 함수를 선언하거나 정의한다.
std::optional<std::uint64_t> extractTotalCount(const Json::Value &body)
// 새 코드 블록 범위를 시작한다.
{
    // 조건을 검사하고 만족 시 분기한다.
    if (!body.isMember("response") || !body["response"].isObject())
    // 새 코드 블록 범위를 시작한다.
    {
        // 현재 함수에서 값을 반환한다.
        return std::nullopt;
    // 현재 코드 블록 범위를 종료한다.
    }

    // 이후 로직에서 사용할 값을 계산하거나 대입한다.
    const auto &responseRoot = body["response"];
    // 조건을 검사하고 만족 시 분기한다.
    if (!responseRoot.isMember("body") || !responseRoot["body"].isObject())
    // 새 코드 블록 범위를 시작한다.
    {
        // 현재 함수에서 값을 반환한다.
        return std::nullopt;
    // 현재 코드 블록 범위를 종료한다.
    }

    // 이후 로직에서 사용할 값을 계산하거나 대입한다.
    const auto &bodyNode = responseRoot["body"];
    // 조건을 검사하고 만족 시 분기한다.
    if (!bodyNode.isMember("totalCount"))
    // 새 코드 블록 범위를 시작한다.
    {
        // 현재 함수에서 값을 반환한다.
        return std::nullopt;
    // 현재 코드 블록 범위를 종료한다.
    }

    // 현재 함수에서 값을 반환한다.
    return optionalJsonUInt64(bodyNode["totalCount"]);
// 현재 코드 블록 범위를 종료한다.
}

// 가공식품 검색 흐름에서 사용하는 함수를 선언하거나 정의한다.
std::vector<Json::Value> extractRows(const Json::Value &root)
// 새 코드 블록 범위를 시작한다.
{
    // 데이터 변환/관리를 위해 표준 라이브러리 유틸리티를 사용한다.
    std::vector<Json::Value> out;

    // 조건을 검사하고 만족 시 분기한다.
    if (root.isMember("data") && root["data"].isArray())
    // 새 코드 블록 범위를 시작한다.
    {
        // 컬렉션 또는 수치 범위를 순회한다.
        for (const auto &item : root["data"])
        // 새 코드 블록 범위를 시작한다.
        {
            // 검색 워크플로우의 다음 구문을 실행한다.
            out.push_back(item);
        // 현재 코드 블록 범위를 종료한다.
        }
        // 현재 함수에서 값을 반환한다.
        return out;
    // 현재 코드 블록 범위를 종료한다.
    }

    // 조건을 검사하고 만족 시 분기한다.
    if (root.isMember("response") && root["response"].isObject())
    // 새 코드 블록 범위를 시작한다.
    {
        // 이후 로직에서 사용할 값을 계산하거나 대입한다.
        const auto &response = root["response"];
        // 조건을 검사하고 만족 시 분기한다.
        if (response.isMember("body") && response["body"].isObject())
        // 새 코드 블록 범위를 시작한다.
        {
            // 이후 로직에서 사용할 값을 계산하거나 대입한다.
            const auto &body = response["body"];
            // 조건을 검사하고 만족 시 분기한다.
            if (body.isMember("items"))
            // 새 코드 블록 범위를 시작한다.
            {
                // 이후 로직에서 사용할 값을 계산하거나 대입한다.
                const auto &items = body["items"];
                // 조건을 검사하고 만족 시 분기한다.
                if (items.isArray())
                // 새 코드 블록 범위를 시작한다.
                {
                    // 컬렉션 또는 수치 범위를 순회한다.
                    for (const auto &item : items)
                    // 새 코드 블록 범위를 시작한다.
                    {
                        // 검색 워크플로우의 다음 구문을 실행한다.
                        out.push_back(item);
                    // 현재 코드 블록 범위를 종료한다.
                    }
                    // 현재 함수에서 값을 반환한다.
                    return out;
                // 현재 코드 블록 범위를 종료한다.
                }

                // 조건을 검사하고 만족 시 분기한다.
                if (items.isObject() && items.isMember("item"))
                // 새 코드 블록 범위를 시작한다.
                {
                    // 이후 로직에서 사용할 값을 계산하거나 대입한다.
                    const auto &itemField = items["item"];
                    // 조건을 검사하고 만족 시 분기한다.
                    if (itemField.isArray())
                    // 새 코드 블록 범위를 시작한다.
                    {
                        // 컬렉션 또는 수치 범위를 순회한다.
                        for (const auto &item : itemField)
                        // 새 코드 블록 범위를 시작한다.
                        {
                            // 검색 워크플로우의 다음 구문을 실행한다.
                            out.push_back(item);
                        // 현재 코드 블록 범위를 종료한다.
                        }
                        // 현재 함수에서 값을 반환한다.
                        return out;
                    // 현재 코드 블록 범위를 종료한다.
                    }

                    // 조건을 검사하고 만족 시 분기한다.
                    if (itemField.isObject())
                    // 새 코드 블록 범위를 시작한다.
                    {
                        // 검색 워크플로우의 다음 구문을 실행한다.
                        out.push_back(itemField);
                        // 현재 함수에서 값을 반환한다.
                        return out;
                    // 현재 코드 블록 범위를 종료한다.
                    }
                // 현재 코드 블록 범위를 종료한다.
                }
            // 현재 코드 블록 범위를 종료한다.
            }
        // 현재 코드 블록 범위를 종료한다.
        }
    // 현재 코드 블록 범위를 종료한다.
    }

    // 현재 함수에서 값을 반환한다.
    return out;
// 현재 코드 블록 범위를 종료한다.
}

// 가공식품 검색 흐름에서 사용하는 함수를 선언하거나 정의한다.
bool looksLikePercentEncoded(const std::string &value)
// 새 코드 블록 범위를 시작한다.
{
    // 컬렉션 또는 수치 범위를 순회한다.
    for (std::size_t i = 0; i + 2 < value.size(); ++i)
    // 새 코드 블록 범위를 시작한다.
    {
        // 조건을 검사하고 만족 시 분기한다.
        if (value[i] != '%')
        // 새 코드 블록 범위를 시작한다.
        {
            // 이번 반복의 나머지를 건너뛰고 다음 반복으로 이동한다.
            continue;
        // 현재 코드 블록 범위를 종료한다.
        }

        // 이후 로직에서 사용할 값을 계산하거나 대입한다.
        const auto h1 = static_cast<unsigned char>(value[i + 1]);
        // 이후 로직에서 사용할 값을 계산하거나 대입한다.
        const auto h2 = static_cast<unsigned char>(value[i + 2]);
        // 조건을 검사하고 만족 시 분기한다.
        if (std::isxdigit(h1) != 0 && std::isxdigit(h2) != 0)
        // 새 코드 블록 범위를 시작한다.
        {
            // 현재 함수에서 값을 반환한다.
            return true;
        // 현재 코드 블록 범위를 종료한다.
        }
    // 현재 코드 블록 범위를 종료한다.
    }
    // 현재 함수에서 값을 반환한다.
    return false;
// 현재 코드 블록 범위를 종료한다.
}

// 가공식품 검색 흐름에서 사용하는 함수를 선언하거나 정의한다.
void appendEncodedQueryParam(std::string &path,
                             // 검색 워크플로우의 다음 구문을 실행한다.
                             const char *key,
                             // 검색 워크플로우의 다음 구문을 실행한다.
                             const std::optional<std::string> &value)
// 새 코드 블록 범위를 시작한다.
{
    // 조건을 검사하고 만족 시 분기한다.
    if (!value.has_value())
    // 새 코드 블록 범위를 시작한다.
    {
        // 현재 함수에서 값을 반환한다.
        return;
    // 현재 코드 블록 범위를 종료한다.
    }

    // 이후 로직에서 사용할 값을 계산하거나 대입한다.
    const auto trimmed = trimCopy(*value);
    // 조건을 검사하고 만족 시 분기한다.
    if (trimmed.empty())
    // 새 코드 블록 범위를 시작한다.
    {
        // 현재 함수에서 값을 반환한다.
        return;
    // 현재 코드 블록 범위를 종료한다.
    }

    // 이후 로직에서 사용할 값을 계산하거나 대입한다.
    const auto encoded = looksLikePercentEncoded(trimmed)
                             // 검색 워크플로우의 다음 구문을 실행한다.
                             ? trimmed
                             // 가공식품 검색 흐름에서 사용하는 함수를 선언하거나 정의한다.
                             : drogon::utils::urlEncodeComponent(trimmed);
    // 이후 로직에서 사용할 값을 계산하거나 대입한다.
    path += "&";
    // 이후 로직에서 사용할 값을 계산하거나 대입한다.
    path += key;
    // 이후 로직에서 사용할 값을 계산하거나 대입한다.
    path += "=";
    // 이후 로직에서 사용할 값을 계산하거나 대입한다.
    path += encoded;
// 현재 코드 블록 범위를 종료한다.
}

// 가공식품 검색 흐름에서 사용하는 함수를 선언하거나 정의한다.
std::string buildPublicNutritionApiPath(
    // 검색 워크플로우의 다음 구문을 실행한다.
    const std::string &serviceKey,
    // 데이터 변환/관리를 위해 표준 라이브러리 유틸리티를 사용한다.
    std::uint64_t pageNo,
    // 데이터 변환/관리를 위해 표준 라이브러리 유틸리티를 사용한다.
    std::uint64_t numOfRows,
    // 검색 워크플로우의 다음 구문을 실행한다.
    const std::optional<std::string> &foodKeyword,
    // 이후 로직에서 사용할 값을 계산하거나 대입한다.
    const std::optional<std::string> &manufacturerName = std::nullopt,
    // 이후 로직에서 사용할 값을 계산하거나 대입한다.
    const std::optional<std::string> &foodLevel4Name = std::nullopt)
// 새 코드 블록 범위를 시작한다.
{
    // 이후 로직에서 사용할 값을 계산하거나 대입한다.
    std::string path =
        // 검색 워크플로우의 다음 구문을 실행한다.
        "/openapi/tn_pubr_public_nutri_process_info_api"
        // 이후 로직에서 사용할 값을 계산하거나 대입한다.
        "?serviceKey=" +
        // 가공식품 검색 흐름에서 사용하는 함수를 선언하거나 정의한다.
        drogon::utils::urlEncodeComponent(serviceKey) + "&type=json&pageNo=" +
        // 가공식품 검색 흐름에서 사용하는 함수를 선언하거나 정의한다.
        std::to_string(pageNo) + "&numOfRows=" + std::to_string(numOfRows);

    // 가공식품 검색 흐름에서 사용하는 함수를 선언하거나 정의한다.
    appendEncodedQueryParam(path, "foodNm", foodKeyword);
    // 가공식품 검색 흐름에서 사용하는 함수를 선언하거나 정의한다.
    appendEncodedQueryParam(path, "mfrNm", manufacturerName);
    // 가공식품 검색 흐름에서 사용하는 함수를 선언하거나 정의한다.
    appendEncodedQueryParam(path, "foodLv4Nm", foodLevel4Name);

    // 현재 함수에서 값을 반환한다.
    return path;
// 현재 코드 블록 범위를 종료한다.
}

// 가공식품 검색 흐름에서 사용하는 함수를 선언하거나 정의한다.
drogon::HttpClientPtr makePublicNutritionApiClient()
// 새 코드 블록 범위를 시작한다.
{
    // 일부 윈도우 환경에서는 드로곤 비동기 리졸버의 호스트 이름 해석이 실패할 수 있다.
    // 현재 함수에서 값을 반환한다.
    return drogon::HttpClient::newHttpClient("https://27.101.215.193",
                                             // 검색 워크플로우의 다음 구문을 실행한다.
                                             nullptr,
                                             // 검색 워크플로우의 다음 구문을 실행한다.
                                             false,
                                             // 검색 워크플로우의 다음 구문을 실행한다.
                                             false);
// 현재 코드 블록 범위를 종료한다.
}

// 검색 워크플로우의 다음 구문을 실행한다.
struct PublicNutritionRowsPage
// 새 코드 블록 범위를 시작한다.
{
    // 데이터 변환/관리를 위해 표준 라이브러리 유틸리티를 사용한다.
    std::vector<Json::Value> rows;
    // 데이터 변환/관리를 위해 표준 라이브러리 유틸리티를 사용한다.
    std::optional<std::uint64_t> totalCount;
// 타입 또는 집합 정의를 닫는다.
};

// 가공식품 검색 흐름에서 사용하는 함수를 선언하거나 정의한다.
std::optional<PublicNutritionRowsPage> requestPublicNutritionRowsPage(
    // 검색 워크플로우의 다음 구문을 실행한다.
    const drogon::HttpClientPtr &client,
    // 검색 워크플로우의 다음 구문을 실행한다.
    const std::string &serviceKey,
    // 데이터 변환/관리를 위해 표준 라이브러리 유틸리티를 사용한다.
    std::uint64_t pageNo,
    // 데이터 변환/관리를 위해 표준 라이브러리 유틸리티를 사용한다.
    std::uint64_t numOfRows,
    // 검색 워크플로우의 다음 구문을 실행한다.
    const std::optional<std::string> &foodKeyword,
    // 검색 워크플로우의 다음 구문을 실행한다.
    const std::optional<std::string> &manufacturerName,
    // 검색 워크플로우의 다음 구문을 실행한다.
    const std::optional<std::string> &foodLevel4Name,
    // 데이터 변환/관리를 위해 표준 라이브러리 유틸리티를 사용한다.
    std::string &errorMessage)
// 새 코드 블록 범위를 시작한다.
{
    // 가공식품 검색 흐름에서 사용하는 함수를 선언하거나 정의한다.
    auto request = drogon::HttpRequest::newHttpRequest();
    // 요청 전송 전에 하이퍼텍스트 전송 프로토콜 요청 필드를 설정한다.
    request->setMethod(drogon::Get);
    // 요청 전송 전에 하이퍼텍스트 전송 프로토콜 요청 필드를 설정한다.
    request->setPathEncode(false);
    // 요청 전송 전에 하이퍼텍스트 전송 프로토콜 요청 필드를 설정한다.
    request->setPath(buildPublicNutritionApiPath(serviceKey,
                                                 // 검색 워크플로우의 다음 구문을 실행한다.
                                                 pageNo,
                                                 // 검색 워크플로우의 다음 구문을 실행한다.
                                                 numOfRows,
                                                 // 검색 워크플로우의 다음 구문을 실행한다.
                                                 foodKeyword,
                                                 // 검색 워크플로우의 다음 구문을 실행한다.
                                                 manufacturerName,
                                                 // 검색 워크플로우의 다음 구문을 실행한다.
                                                 foodLevel4Name));
    // 요청 전송 전에 하이퍼텍스트 전송 프로토콜 요청 필드를 설정한다.
    request->addHeader("Host", "api.data.go.kr");

    // 이후 로직에서 사용할 값을 계산하거나 대입한다.
    const auto [reqResult, response] = client->sendRequest(request, 20.0);
    // 조건을 검사하고 만족 시 분기한다.
    if (reqResult != drogon::ReqResult::Ok || !response)
    // 새 코드 블록 범위를 시작한다.
    {
        // 이후 로직에서 사용할 값을 계산하거나 대입한다.
        errorMessage = "Failed to call nutrition public API. (ReqResult: " +
                       // 가공식품 검색 흐름에서 사용하는 함수를 선언하거나 정의한다.
                       drogon::to_string(reqResult) + ")";
        // 현재 함수에서 값을 반환한다.
        return std::nullopt;
    // 현재 코드 블록 범위를 종료한다.
    }

    // 조건을 검사하고 만족 시 분기한다.
    if (response->statusCode() != drogon::k200OK)
    // 새 코드 블록 범위를 시작한다.
    {
        // 이후 로직에서 사용할 값을 계산하거나 대입한다.
        errorMessage = "Nutrition public API returned error.";
        // 현재 함수에서 값을 반환한다.
        return std::nullopt;
    // 현재 코드 블록 범위를 종료한다.
    }

    // 이후 로직에서 사용할 값을 계산하거나 대입한다.
    const auto body = response->getJsonObject();
    // 조건을 검사하고 만족 시 분기한다.
    if (!body)
    // 새 코드 블록 범위를 시작한다.
    {
        // 이후 로직에서 사용할 값을 계산하거나 대입한다.
        errorMessage = "Invalid response from nutrition public API.";
        // 현재 함수에서 값을 반환한다.
        return std::nullopt;
    // 현재 코드 블록 범위를 종료한다.
    }

    // 조건을 검사하고 만족 시 분기한다.
    if (const auto apiError = extractPublicApiErrorMessage(*body); apiError.has_value())
    // 새 코드 블록 범위를 시작한다.
    {
        // 조건을 검사하고 만족 시 분기한다.
        if (apiError->find("Nutrition public API error (03)") != std::string::npos)
        // 새 코드 블록 범위를 시작한다.
        {
            // 검색 워크플로우의 다음 구문을 실행한다.
            PublicNutritionRowsPage emptyPage;
            // 이후 로직에서 사용할 값을 계산하거나 대입한다.
            emptyPage.totalCount = 0U;
            // 현재 함수에서 값을 반환한다.
            return emptyPage;
        // 현재 코드 블록 범위를 종료한다.
        }

        // 이후 로직에서 사용할 값을 계산하거나 대입한다.
        errorMessage = *apiError;
        // 현재 함수에서 값을 반환한다.
        return std::nullopt;
    // 현재 코드 블록 범위를 종료한다.
    }

    // 검색 워크플로우의 다음 구문을 실행한다.
    PublicNutritionRowsPage page;
    // 이후 로직에서 사용할 값을 계산하거나 대입한다.
    page.rows = extractRows(*body);
    // 이후 로직에서 사용할 값을 계산하거나 대입한다.
    page.totalCount = extractTotalCount(*body);
    // 현재 함수에서 값을 반환한다.
    return page;
// 현재 코드 블록 범위를 종료한다.
}

// 가공식품 검색 흐름에서 사용하는 함수를 선언하거나 정의한다.
std::vector<ingredient::ProcessedFoodSearchItemDTO> mapRowsToFoods(
    // 검색 워크플로우의 다음 구문을 실행한다.
    const std::vector<Json::Value> &rows,
    // 검색 워크플로우의 다음 구문을 실행한다.
    bool dedupeByFoodName)
// 새 코드 블록 범위를 시작한다.
{
    // 데이터 변환/관리를 위해 표준 라이브러리 유틸리티를 사용한다.
    std::vector<ingredient::ProcessedFoodSearchItemDTO> foods;
    // 검색 워크플로우의 다음 구문을 실행한다.
    foods.reserve(rows.size());

    // 데이터 변환/관리를 위해 표준 라이브러리 유틸리티를 사용한다.
    std::unordered_set<std::string> seenFoodNames;
    // 조건을 검사하고 만족 시 분기한다.
    if (dedupeByFoodName)
    // 새 코드 블록 범위를 시작한다.
    {
        // 검색 워크플로우의 다음 구문을 실행한다.
        seenFoodNames.reserve(rows.size());
    // 현재 코드 블록 범위를 종료한다.
    }

    // 컬렉션 또는 수치 범위를 순회한다.
    for (const auto &row : rows)
    // 새 코드 블록 범위를 시작한다.
    {
        // 이후 로직에서 사용할 값을 계산하거나 대입한다.
        auto foodCode = firstStringFromKeys(row, {"foodCd", "FOOD_CD"});
        // 이후 로직에서 사용할 값을 계산하거나 대입한다.
        auto foodName = firstStringFromKeys(row, {"foodNm", "FOOD_NM"});
        // 조건을 검사하고 만족 시 분기한다.
        if (!foodCode.has_value() || !foodName.has_value())
        // 새 코드 블록 범위를 시작한다.
        {
            // 이번 반복의 나머지를 건너뛰고 다음 반복으로 이동한다.
            continue;
        // 현재 코드 블록 범위를 종료한다.
        }

        // 조건을 검사하고 만족 시 분기한다.
        if (dedupeByFoodName && !seenFoodNames.insert(*foodName).second)
        // 새 코드 블록 범위를 시작한다.
        {
            // 이번 반복의 나머지를 건너뛰고 다음 반복으로 이동한다.
            continue;
        // 현재 코드 블록 범위를 종료한다.
        }

        // 검색 워크플로우의 다음 구문을 실행한다.
        ingredient::ProcessedFoodSearchItemDTO dto;
        // 이후 로직에서 사용할 값을 계산하거나 대입한다.
        dto.foodCode = *foodCode;
        // 이후 로직에서 사용할 값을 계산하거나 대입한다.
        dto.foodName = *foodName;
        // 이후 로직에서 사용할 값을 계산하거나 대입한다.
        dto.foodGroupName = firstStringFromKeys(
            // 검색 워크플로우의 다음 구문을 실행한다.
            row,
            // 검색 워크플로우의 다음 구문을 실행한다.
            {"foodLv6Nm", "foodLv5Nm", "foodLv4Nm", "foodLv3Nm",
             // 검색 워크플로우의 다음 구문을 실행한다.
             "foodLv7Nm", "FOOD_LV6_NM", "FOOD_LV5_NM", "FOOD_LV4_NM",
             // 검색 워크플로우의 다음 구문을 실행한다.
             "FOOD_LV3_NM", "FOOD_LV7_NM"});
        // 이후 로직에서 사용할 값을 계산하거나 대입한다.
        dto.nutritionBasisAmount =
            // 가공식품 검색 흐름에서 사용하는 함수를 선언하거나 정의한다.
            firstStringFromKeys(row, {"nutConSrtrQua", "NUT_CON_SRTR_QUA"});
        // 이후 로직에서 사용할 값을 계산하거나 대입한다.
        dto.energyKcal = firstDoubleFromKeys(row, {"enerc", "ENERC"});
        // 이후 로직에서 사용할 값을 계산하거나 대입한다.
        dto.proteinG = firstDoubleFromKeys(row, {"prot", "PROT"});
        // 이후 로직에서 사용할 값을 계산하거나 대입한다.
        dto.fatG = firstDoubleFromKeys(row, {"fatce", "FATCE"});
        // 이후 로직에서 사용할 값을 계산하거나 대입한다.
        dto.carbohydrateG = firstDoubleFromKeys(row, {"chocdf", "CHOCDF"});
        // 이후 로직에서 사용할 값을 계산하거나 대입한다.
        dto.sugarG = firstDoubleFromKeys(row, {"sugar", "SUGAR"});
        // 이후 로직에서 사용할 값을 계산하거나 대입한다.
        dto.sodiumMg = firstDoubleFromKeys(row, {"nat", "NAT"});
        // 이후 로직에서 사용할 값을 계산하거나 대입한다.
        dto.sourceName = firstStringFromKeys(row, {"srcNm", "SRC_NM"});
        // 이후 로직에서 사용할 값을 계산하거나 대입한다.
        dto.manufacturerName = firstStringFromKeys(row, {"mfrNm", "MFR_NM"});
        // 이후 로직에서 사용할 값을 계산하거나 대입한다.
        dto.importYn =
            // 가공식품 검색 흐름에서 사용하는 함수를 선언하거나 정의한다.
            normalizeImportYnForApi(firstStringFromKeys(row, {"imptYn", "IMPT_YN"}));
        // 이후 로직에서 사용할 값을 계산하거나 대입한다.
        dto.originCountryName = firstStringFromKeys(row, {"cooNm", "COO_NM"});
        // 이후 로직에서 사용할 값을 계산하거나 대입한다.
        dto.dataBaseDate =
            // 가공식품 검색 흐름에서 사용하는 함수를 선언하거나 정의한다.
            normalizeDataBaseDate(firstStringFromKeys(row, {"crtrYmd", "CRTR_YMD"}));
        // 가공식품 검색 흐름에서 사용하는 함수를 선언하거나 정의한다.
        foods.push_back(std::move(dto));
    // 현재 코드 블록 범위를 종료한다.
    }

    // 현재 함수에서 값을 반환한다.
    return foods;
// 현재 코드 블록 범위를 종료한다.
}

// 가공식품 검색 흐름에서 사용하는 함수를 선언하거나 정의한다.
int searchPriority(const ingredient::ProcessedFoodSearchItemDTO &food,
                   // 검색 워크플로우의 다음 구문을 실행한다.
                   const std::string &normalizedKeywordToken)
// 새 코드 블록 범위를 시작한다.
{
    // 조건을 검사하고 만족 시 분기한다.
    if (normalizedKeywordToken.empty())
    // 새 코드 블록 범위를 시작한다.
    {
        // 현재 함수에서 값을 반환한다.
        return 2;
    // 현재 코드 블록 범위를 종료한다.
    }

    // 이후 로직에서 사용할 값을 계산하거나 대입한다.
    auto normalizedFoodName = trimCopy(food.foodName);
    // 가공식품 검색 흐름에서 사용하는 함수를 선언하거나 정의한다.
    normalizedFoodName = toLowerAscii(std::move(normalizedFoodName));
    // 검색 워크플로우의 다음 구문을 실행한다.
    normalizedFoodName.erase(
        // 가공식품 검색 흐름에서 사용하는 함수를 선언하거나 정의한다.
        std::remove_if(normalizedFoodName.begin(),
                       // 검색 워크플로우의 다음 구문을 실행한다.
                       normalizedFoodName.end(),
                       // 검색 워크플로우의 다음 구문을 실행한다.
                       [](unsigned char ch) {
                           // 현재 함수에서 값을 반환한다.
                           return std::isspace(ch) != 0;
                       // 검색 워크플로우의 다음 구문을 실행한다.
                       }),
        // 검색 워크플로우의 다음 구문을 실행한다.
        normalizedFoodName.end());

    // 조건을 검사하고 만족 시 분기한다.
    if (normalizedFoodName == normalizedKeywordToken)
    // 새 코드 블록 범위를 시작한다.
    {
        // 현재 함수에서 값을 반환한다.
        return 0;
    // 현재 코드 블록 범위를 종료한다.
    }
    // 조건을 검사하고 만족 시 분기한다.
    if (normalizedFoodName.rfind(normalizedKeywordToken, 0) == 0)
    // 새 코드 블록 범위를 시작한다.
    {
        // 현재 함수에서 값을 반환한다.
        return 1;
    // 현재 코드 블록 범위를 종료한다.
    }
    // 현재 함수에서 값을 반환한다.
    return 2;
// 현재 코드 블록 범위를 종료한다.
}

// 검색 워크플로우의 다음 구문을 실행한다.
struct CacheEntry
// 새 코드 블록 범위를 시작한다.
{
    // 데이터 변환/관리를 위해 표준 라이브러리 유틸리티를 사용한다.
    std::chrono::steady_clock::time_point expiresAt;
    // 검색 워크플로우의 다음 구문을 실행한다.
    ingredient::ProcessedFoodSearchResultDTO result;
// 타입 또는 집합 정의를 닫는다.
};

// 검색 흐름에서 사용하는 컴파일 타임 상수를 정의한다.
constexpr std::chrono::seconds kSearchCacheTtl(120);
// 데이터 변환/관리를 위해 표준 라이브러리 유틸리티를 사용한다.
std::mutex gSearchCacheMutex;
// 데이터 변환/관리를 위해 표준 라이브러리 유틸리티를 사용한다.
std::unordered_map<std::string, CacheEntry> gSearchCache;

// 가공식품 검색 흐름에서 사용하는 함수를 선언하거나 정의한다.
std::optional<ingredient::ProcessedFoodSearchResultDTO> getCachedSearchResult(
    // 검색 워크플로우의 다음 구문을 실행한다.
    const std::string &normalizedKeyword)
// 새 코드 블록 범위를 시작한다.
{
    // 공유 상태를 스레드 안전하게 접근하기 위해 뮤텍스를 잠근다.
    std::lock_guard<std::mutex> lock(gSearchCacheMutex);
    // 이후 로직에서 사용할 값을 계산하거나 대입한다.
    const auto it = gSearchCache.find(normalizedKeyword);
    // 조건을 검사하고 만족 시 분기한다.
    if (it == gSearchCache.end())
    // 새 코드 블록 범위를 시작한다.
    {
        // 현재 함수에서 값을 반환한다.
        return std::nullopt;
    // 현재 코드 블록 범위를 종료한다.
    }

    // 조건을 검사하고 만족 시 분기한다.
    if (std::chrono::steady_clock::now() >= it->second.expiresAt)
    // 새 코드 블록 범위를 시작한다.
    {
        // 검색 워크플로우의 다음 구문을 실행한다.
        gSearchCache.erase(it);
        // 현재 함수에서 값을 반환한다.
        return std::nullopt;
    // 현재 코드 블록 범위를 종료한다.
    }

    // 현재 함수에서 값을 반환한다.
    return it->second.result;
// 현재 코드 블록 범위를 종료한다.
}

// 가공식품 검색 흐름에서 사용하는 함수를 선언하거나 정의한다.
void cacheSearchResult(const std::string &normalizedKeyword,
                       // 검색 워크플로우의 다음 구문을 실행한다.
                       const ingredient::ProcessedFoodSearchResultDTO &result)
// 새 코드 블록 범위를 시작한다.
{
    // 공유 상태를 스레드 안전하게 접근하기 위해 뮤텍스를 잠근다.
    std::lock_guard<std::mutex> lock(gSearchCacheMutex);
    // 최신 결과를 메모리 캐시 맵에 기록한다.
    gSearchCache[normalizedKeyword] = CacheEntry{
        // 가공식품 검색 흐름에서 사용하는 함수를 선언하거나 정의한다.
        std::chrono::steady_clock::now() + kSearchCacheTtl,
        // 검색 워크플로우의 다음 구문을 실행한다.
        result,
    // 타입 또는 집합 정의를 닫는다.
    };
// 현재 코드 블록 범위를 종료한다.
}

// 현재 코드 블록 범위를 종료한다.
}  // 익명 네임스페이스 종료

// 심볼 가시성 제어를 위해 네임스페이스 범위를 연다.
namespace ingredient
// 새 코드 블록 범위를 시작한다.
{

// 가공식품 검색 흐름에서 사용하는 함수를 선언하거나 정의한다.
void ProcessedFoodSearchService::searchProcessedFoods(
    // 검색 워크플로우의 다음 구문을 실행한다.
    const std::string &keyword,
    // 검색 워크플로우의 다음 구문을 실행한다.
    ProcessedFoodSearchCallback &&callback)
// 새 코드 블록 범위를 시작한다.
{
    // 검색 워크플로우의 다음 구문을 실행한다.
    ProcessedFoodSearchResultDTO invalidInput;
    // 이후 로직에서 사용할 값을 계산하거나 대입한다.
    const auto normalizedKeyword = trim(keyword);
    // 조건을 검사하고 만족 시 분기한다.
    if (normalizedKeyword.empty())
    // 새 코드 블록 범위를 시작한다.
    {
        // 이후 로직에서 사용할 값을 계산하거나 대입한다.
        invalidInput.statusCode = 400;
        // 이후 로직에서 사용할 값을 계산하거나 대입한다.
        invalidInput.message = "Keyword is required.";
        // 현재 결과로 완료 콜백을 호출한다.
        callback(std::move(invalidInput));
        // 현재 함수에서 값을 반환한다.
        return;
    // 현재 코드 블록 범위를 종료한다.
    }

    // 이후 로직에서 사용할 값을 계산하거나 대입한다.
    const auto normalizedKeywordToken = normalizeForSearch(normalizedKeyword);
    // 조건을 검사하고 만족 시 분기한다.
    if (const auto cached = getCachedSearchResult(normalizedKeywordToken);
        // 검색 워크플로우의 다음 구문을 실행한다.
        cached.has_value())
    // 새 코드 블록 범위를 시작한다.
    {
        // 현재 결과로 완료 콜백을 호출한다.
        callback(*cached);
        // 현재 함수에서 값을 반환한다.
        return;
    // 현재 코드 블록 범위를 종료한다.
    }

    // 이후 로직에서 사용할 값을 계산하거나 대입한다.
    auto serviceKey = getEnvValue("PUBLIC_NUTRI_SERVICE_KEY");
    // 조건을 검사하고 만족 시 분기한다.
    if (!serviceKey.has_value())
    // 새 코드 블록 범위를 시작한다.
    {
        // 이후 로직에서 사용할 값을 계산하거나 대입한다.
        serviceKey = getServiceKeyFromLocalFile();
    // 현재 코드 블록 범위를 종료한다.
    }

    // 조건을 검사하고 만족 시 분기한다.
    if (!serviceKey.has_value())
    // 새 코드 블록 범위를 시작한다.
    {
        // 검색 워크플로우의 다음 구문을 실행한다.
        ProcessedFoodSearchResultDTO keyError;
        // 이후 로직에서 사용할 값을 계산하거나 대입한다.
        keyError.statusCode = 500;
        // 이후 로직에서 사용할 값을 계산하거나 대입한다.
        keyError.message =
            // 검색 워크플로우의 다음 구문을 실행한다.
            "PUBLIC_NUTRI_SERVICE_KEY is not configured. "
            // 검색 워크플로우의 다음 구문을 실행한다.
            "Set env var or backend/config/keys.local.json.";
        // 현재 결과로 완료 콜백을 호출한다.
        callback(std::move(keyError));
        // 현재 함수에서 값을 반환한다.
        return;
    // 현재 코드 블록 범위를 종료한다.
    }

    // 분리된 백그라운드 스레드에서 비동기 작업을 시작한다.
    std::thread([serviceKey = *serviceKey,
                 // 검색 워크플로우의 다음 구문을 실행한다.
                 normalizedKeyword,
                 // 검색 워크플로우의 다음 구문을 실행한다.
                 normalizedKeywordToken,
                 // 가공식품 검색 흐름에서 사용하는 함수를 선언하거나 정의한다.
                 callback = std::move(callback)]() mutable {
        // 검색 워크플로우의 다음 구문을 실행한다.
        ProcessedFoodSearchResultDTO result;
        // 이후 로직에서 사용할 값을 계산하거나 대입한다.
        const auto client = makePublicNutritionApiClient();
        // 조건을 검사하고 만족 시 분기한다.
        if (!client)
        // 새 코드 블록 범위를 시작한다.
        {
            // 이후 로직에서 사용할 값을 계산하거나 대입한다.
            result.statusCode = 502;
            // 이후 로직에서 사용할 값을 계산하거나 대입한다.
            result.message = "Failed to call nutrition public API.";
            // 현재 결과로 완료 콜백을 호출한다.
            callback(std::move(result));
            // 현재 함수에서 값을 반환한다.
            return;
        // 현재 코드 블록 범위를 종료한다.
        }

        // 데이터 변환/관리를 위해 표준 라이브러리 유틸리티를 사용한다.
        std::string errorMessage;
        // 검색 흐름에서 사용하는 컴파일 타임 상수를 정의한다.
        constexpr std::uint64_t kPrimaryRows = 30;
        // 이후 로직에서 사용할 값을 계산하거나 대입한다.
        const auto primaryPage = requestPublicNutritionRowsPage(client,
                                                                // 검색 워크플로우의 다음 구문을 실행한다.
                                                                serviceKey,
                                                                // 검색 워크플로우의 다음 구문을 실행한다.
                                                                1,
                                                                // 검색 워크플로우의 다음 구문을 실행한다.
                                                                kPrimaryRows,
                                                                // 검색 워크플로우의 다음 구문을 실행한다.
                                                                normalizedKeyword,
                                                                // 데이터 변환/관리를 위해 표준 라이브러리 유틸리티를 사용한다.
                                                                std::nullopt,
                                                                // 데이터 변환/관리를 위해 표준 라이브러리 유틸리티를 사용한다.
                                                                std::nullopt,
                                                                // 검색 워크플로우의 다음 구문을 실행한다.
                                                                errorMessage);
        // 조건을 검사하고 만족 시 분기한다.
        if (!primaryPage.has_value())
        // 새 코드 블록 범위를 시작한다.
        {
            // 이후 로직에서 사용할 값을 계산하거나 대입한다.
            result.statusCode = 502;
            // 이후 로직에서 사용할 값을 계산하거나 대입한다.
            result.message =
                // 검색 워크플로우의 다음 구문을 실행한다.
                errorMessage.empty() ? "Failed to call nutrition public API."
                                     // 검색 워크플로우의 다음 구문을 실행한다.
                                     : errorMessage;
            // 현재 결과로 완료 콜백을 호출한다.
            callback(std::move(result));
            // 현재 함수에서 값을 반환한다.
            return;
        // 현재 코드 블록 범위를 종료한다.
        }

        // 데이터 변환/관리를 위해 표준 라이브러리 유틸리티를 사용한다.
        std::vector<Json::Value> mergedRows;
        // 검색 워크플로우의 다음 구문을 실행한다.
        mergedRows.reserve(primaryPage->rows.size() + 64U);

        // 데이터 변환/관리를 위해 표준 라이브러리 유틸리티를 사용한다.
        std::unordered_set<std::string> seenFoodCodes;
        // 검색 워크플로우의 다음 구문을 실행한다.
        seenFoodCodes.reserve(primaryPage->rows.size() + 64U);

        // 이후 로직에서 사용할 값을 계산하거나 대입한다.
        auto appendRows = [&](const std::vector<Json::Value> &rows,
                              // 검색 워크플로우의 다음 구문을 실행한다.
                              bool filterByKeyword) {
            // 컬렉션 또는 수치 범위를 순회한다.
            for (const auto &row : rows)
            // 새 코드 블록 범위를 시작한다.
            {
                // 이후 로직에서 사용할 값을 계산하거나 대입한다.
                const auto foodCode = firstStringFromKeys(row, {"foodCd", "FOOD_CD"});
                // 이후 로직에서 사용할 값을 계산하거나 대입한다.
                const auto foodName = firstStringFromKeys(row, {"foodNm", "FOOD_NM"});
                // 조건을 검사하고 만족 시 분기한다.
                if (!foodCode.has_value() || !foodName.has_value())
                // 새 코드 블록 범위를 시작한다.
                {
                    // 이번 반복의 나머지를 건너뛰고 다음 반복으로 이동한다.
                    continue;
                // 현재 코드 블록 범위를 종료한다.
                }

                // 조건을 검사하고 만족 시 분기한다.
                if (filterByKeyword)
                // 새 코드 블록 범위를 시작한다.
                {
                    // 이후 로직에서 사용할 값을 계산하거나 대입한다.
                    auto normalizedFoodName = trimCopy(*foodName);
                    // 가공식품 검색 흐름에서 사용하는 함수를 선언하거나 정의한다.
                    normalizedFoodName = toLowerAscii(std::move(normalizedFoodName));
                    // 검색 워크플로우의 다음 구문을 실행한다.
                    normalizedFoodName.erase(
                        // 가공식품 검색 흐름에서 사용하는 함수를 선언하거나 정의한다.
                        std::remove_if(normalizedFoodName.begin(),
                                       // 검색 워크플로우의 다음 구문을 실행한다.
                                       normalizedFoodName.end(),
                                       // 검색 워크플로우의 다음 구문을 실행한다.
                                       [](unsigned char ch) {
                                           // 현재 함수에서 값을 반환한다.
                                           return std::isspace(ch) != 0;
                                       // 검색 워크플로우의 다음 구문을 실행한다.
                                       }),
                        // 검색 워크플로우의 다음 구문을 실행한다.
                        normalizedFoodName.end());
                    // 조건을 검사하고 만족 시 분기한다.
                    if (normalizedFoodName.find(normalizedKeywordToken) ==
                        // 데이터 변환/관리를 위해 표준 라이브러리 유틸리티를 사용한다.
                        std::string::npos)
                    // 새 코드 블록 범위를 시작한다.
                    {
                        // 이번 반복의 나머지를 건너뛰고 다음 반복으로 이동한다.
                        continue;
                    // 현재 코드 블록 범위를 종료한다.
                    }
                // 현재 코드 블록 범위를 종료한다.
                }

                // 조건을 검사하고 만족 시 분기한다.
                if (!seenFoodCodes.insert(*foodCode).second)
                // 새 코드 블록 범위를 시작한다.
                {
                    // 이번 반복의 나머지를 건너뛰고 다음 반복으로 이동한다.
                    continue;
                // 현재 코드 블록 범위를 종료한다.
                }

                // 검색 워크플로우의 다음 구문을 실행한다.
                mergedRows.push_back(row);
            // 현재 코드 블록 범위를 종료한다.
            }
        // 타입 또는 집합 정의를 닫는다.
        };

        // 가공식품 검색 흐름에서 사용하는 함수를 선언하거나 정의한다.
        appendRows(primaryPage->rows, false);

        // 데이터 변환/관리를 위해 표준 라이브러리 유틸리티를 사용한다.
        std::unordered_set<std::string> manufacturerNames;
        // 데이터 변환/관리를 위해 표준 라이브러리 유틸리티를 사용한다.
        std::unordered_set<std::string> foodLevel4Names;
        // 컬렉션 또는 수치 범위를 순회한다.
        for (const auto &row : primaryPage->rows)
        // 새 코드 블록 범위를 시작한다.
        {
            // 조건을 검사하고 만족 시 분기한다.
            if (const auto manufacturerName =
                    // 가공식품 검색 흐름에서 사용하는 함수를 선언하거나 정의한다.
                    firstStringFromKeys(row, {"mfrNm", "MFR_NM"});
                // 검색 워크플로우의 다음 구문을 실행한다.
                manufacturerName.has_value())
            // 새 코드 블록 범위를 시작한다.
            {
                // 이후 로직에서 사용할 값을 계산하거나 대입한다.
                const auto normalized = trimCopy(*manufacturerName);
                // 조건을 검사하고 만족 시 분기한다.
                if (!normalized.empty())
                // 새 코드 블록 범위를 시작한다.
                {
                    // 검색 워크플로우의 다음 구문을 실행한다.
                    manufacturerNames.insert(normalized);
                // 현재 코드 블록 범위를 종료한다.
                }
            // 현재 코드 블록 범위를 종료한다.
            }

            // 조건을 검사하고 만족 시 분기한다.
            if (const auto foodLevel4Name =
                    // 가공식품 검색 흐름에서 사용하는 함수를 선언하거나 정의한다.
                    firstStringFromKeys(row, {"foodLv4Nm", "FOOD_LV4_NM"});
                // 검색 워크플로우의 다음 구문을 실행한다.
                foodLevel4Name.has_value())
            // 새 코드 블록 범위를 시작한다.
            {
                // 이후 로직에서 사용할 값을 계산하거나 대입한다.
                const auto normalized = trimCopy(*foodLevel4Name);
                // 조건을 검사하고 만족 시 분기한다.
                if (!normalized.empty())
                // 새 코드 블록 범위를 시작한다.
                {
                    // 검색 워크플로우의 다음 구문을 실행한다.
                    foodLevel4Names.insert(normalized);
                // 현재 코드 블록 범위를 종료한다.
                }
            // 현재 코드 블록 범위를 종료한다.
            }
        // 현재 코드 블록 범위를 종료한다.
        }

        // 검색 흐름에서 사용하는 컴파일 타임 상수를 정의한다.
        constexpr std::size_t kMaxRelatedCombinations = 3U;
        // 검색 흐름에서 사용하는 컴파일 타임 상수를 정의한다.
        constexpr std::uint64_t kRelatedRowsPerPage = 100U;
        // 검색 흐름에서 사용하는 컴파일 타임 상수를 정의한다.
        constexpr std::uint64_t kMaxRelatedPages = 8U;
        // 이후 로직에서 사용할 값을 계산하거나 대입한다.
        std::size_t processedCombinations = 0U;

        // 컬렉션 또는 수치 범위를 순회한다.
        for (const auto &manufacturerName : manufacturerNames)
        // 새 코드 블록 범위를 시작한다.
        {
            // 조건을 검사하고 만족 시 분기한다.
            if (processedCombinations >= kMaxRelatedCombinations)
            // 새 코드 블록 범위를 시작한다.
            {
                // 현재 루프 또는 스위치 블록을 즉시 종료한다.
                break;
            // 현재 코드 블록 범위를 종료한다.
            }

            // 조건을 검사하고 만족 시 분기한다.
            if (foodLevel4Names.empty())
            // 새 코드 블록 범위를 시작한다.
            {
                // 검색 워크플로우의 다음 구문을 실행한다.
                ++processedCombinations;

                // 컬렉션 또는 수치 범위를 순회한다.
                for (std::uint64_t pageNo = 1; pageNo <= kMaxRelatedPages; ++pageNo)
                // 새 코드 블록 범위를 시작한다.
                {
                    // 데이터 변환/관리를 위해 표준 라이브러리 유틸리티를 사용한다.
                    std::string relatedError;
                    // 이후 로직에서 사용할 값을 계산하거나 대입한다.
                    const auto relatedPage = requestPublicNutritionRowsPage(
                        // 검색 워크플로우의 다음 구문을 실행한다.
                        client,
                        // 검색 워크플로우의 다음 구문을 실행한다.
                        serviceKey,
                        // 검색 워크플로우의 다음 구문을 실행한다.
                        pageNo,
                        // 검색 워크플로우의 다음 구문을 실행한다.
                        kRelatedRowsPerPage,
                        // 데이터 변환/관리를 위해 표준 라이브러리 유틸리티를 사용한다.
                        std::nullopt,
                        // 검색 워크플로우의 다음 구문을 실행한다.
                        manufacturerName,
                        // 데이터 변환/관리를 위해 표준 라이브러리 유틸리티를 사용한다.
                        std::nullopt,
                        // 검색 워크플로우의 다음 구문을 실행한다.
                        relatedError);
                    // 조건을 검사하고 만족 시 분기한다.
                    if (!relatedPage.has_value())
                    // 새 코드 블록 범위를 시작한다.
                    {
                        // 현재 루프 또는 스위치 블록을 즉시 종료한다.
                        break;
                    // 현재 코드 블록 범위를 종료한다.
                    }

                    // 가공식품 검색 흐름에서 사용하는 함수를 선언하거나 정의한다.
                    appendRows(relatedPage->rows, true);

                    // 조건을 검사하고 만족 시 분기한다.
                    if (relatedPage->rows.size() < kRelatedRowsPerPage)
                    // 새 코드 블록 범위를 시작한다.
                    {
                        // 현재 루프 또는 스위치 블록을 즉시 종료한다.
                        break;
                    // 현재 코드 블록 범위를 종료한다.
                    }
                // 현재 코드 블록 범위를 종료한다.
                }
                // 이번 반복의 나머지를 건너뛰고 다음 반복으로 이동한다.
                continue;
            // 현재 코드 블록 범위를 종료한다.
            }

            // 컬렉션 또는 수치 범위를 순회한다.
            for (const auto &foodLevel4Name : foodLevel4Names)
            // 새 코드 블록 범위를 시작한다.
            {
                // 조건을 검사하고 만족 시 분기한다.
                if (processedCombinations >= kMaxRelatedCombinations)
                // 새 코드 블록 범위를 시작한다.
                {
                    // 현재 루프 또는 스위치 블록을 즉시 종료한다.
                    break;
                // 현재 코드 블록 범위를 종료한다.
                }
                // 검색 워크플로우의 다음 구문을 실행한다.
                ++processedCombinations;

                // 컬렉션 또는 수치 범위를 순회한다.
                for (std::uint64_t pageNo = 1; pageNo <= kMaxRelatedPages; ++pageNo)
                // 새 코드 블록 범위를 시작한다.
                {
                    // 데이터 변환/관리를 위해 표준 라이브러리 유틸리티를 사용한다.
                    std::string relatedError;
                    // 이후 로직에서 사용할 값을 계산하거나 대입한다.
                    const auto relatedPage = requestPublicNutritionRowsPage(
                        // 검색 워크플로우의 다음 구문을 실행한다.
                        client,
                        // 검색 워크플로우의 다음 구문을 실행한다.
                        serviceKey,
                        // 검색 워크플로우의 다음 구문을 실행한다.
                        pageNo,
                        // 검색 워크플로우의 다음 구문을 실행한다.
                        kRelatedRowsPerPage,
                        // 데이터 변환/관리를 위해 표준 라이브러리 유틸리티를 사용한다.
                        std::nullopt,
                        // 검색 워크플로우의 다음 구문을 실행한다.
                        manufacturerName,
                        // 검색 워크플로우의 다음 구문을 실행한다.
                        foodLevel4Name,
                        // 검색 워크플로우의 다음 구문을 실행한다.
                        relatedError);
                    // 조건을 검사하고 만족 시 분기한다.
                    if (!relatedPage.has_value())
                    // 새 코드 블록 범위를 시작한다.
                    {
                        // 현재 루프 또는 스위치 블록을 즉시 종료한다.
                        break;
                    // 현재 코드 블록 범위를 종료한다.
                    }

                    // 가공식품 검색 흐름에서 사용하는 함수를 선언하거나 정의한다.
                    appendRows(relatedPage->rows, true);

                    // 조건을 검사하고 만족 시 분기한다.
                    if (relatedPage->rows.size() < kRelatedRowsPerPage)
                    // 새 코드 블록 범위를 시작한다.
                    {
                        // 현재 루프 또는 스위치 블록을 즉시 종료한다.
                        break;
                    // 현재 코드 블록 범위를 종료한다.
                    }
                // 현재 코드 블록 범위를 종료한다.
                }
            // 현재 코드 블록 범위를 종료한다.
            }
        // 현재 코드 블록 범위를 종료한다.
        }

        // 이후 로직에서 사용할 값을 계산하거나 대입한다.
        auto foods = mapRowsToFoods(mergedRows, true);
        // 가공식품 검색 흐름에서 사용하는 함수를 선언하거나 정의한다.
        std::sort(foods.begin(),
                  // 검색 워크플로우의 다음 구문을 실행한다.
                  foods.end(),
                  // 검색 워크플로우의 다음 구문을 실행한다.
                  [&normalizedKeywordToken](const ProcessedFoodSearchItemDTO &lhs,
                                            // 검색 워크플로우의 다음 구문을 실행한다.
                                            const ProcessedFoodSearchItemDTO &rhs) {
                      // 이후 로직에서 사용할 값을 계산하거나 대입한다.
                      const auto lhsPriority = searchPriority(lhs, normalizedKeywordToken);
                      // 이후 로직에서 사용할 값을 계산하거나 대입한다.
                      const auto rhsPriority = searchPriority(rhs, normalizedKeywordToken);
                      // 조건을 검사하고 만족 시 분기한다.
                      if (lhsPriority != rhsPriority)
                      // 새 코드 블록 범위를 시작한다.
                      {
                          // 현재 함수에서 값을 반환한다.
                          return lhsPriority < rhsPriority;
                      // 현재 코드 블록 범위를 종료한다.
                      }
                      // 현재 함수에서 값을 반환한다.
                      return lhs.foodName < rhs.foodName;
                  // 검색 워크플로우의 다음 구문을 실행한다.
                  });

        // 검색 흐름에서 사용하는 컴파일 타임 상수를 정의한다.
        constexpr std::size_t kMaxResultCount = 30U;
        // 조건을 검사하고 만족 시 분기한다.
        if (foods.size() > kMaxResultCount)
        // 새 코드 블록 범위를 시작한다.
        {
            // 검색 워크플로우의 다음 구문을 실행한다.
            foods.resize(kMaxResultCount);
        // 현재 코드 블록 범위를 종료한다.
        }

        // 이후 로직에서 사용할 값을 계산하거나 대입한다.
        result.ok = true;
        // 이후 로직에서 사용할 값을 계산하거나 대입한다.
        result.statusCode = 200;
        // 이후 로직에서 사용할 값을 계산하거나 대입한다.
        result.message = "Nutrition foods loaded.";
        // 가공식품 검색 흐름에서 사용하는 함수를 선언하거나 정의한다.
        result.foods = std::move(foods);
        // 가공식품 검색 흐름에서 사용하는 함수를 선언하거나 정의한다.
        cacheSearchResult(normalizedKeywordToken, result);

        // 현재 결과로 완료 콜백을 호출한다.
        callback(std::move(result));
    // 검색 워크플로우의 다음 구문을 실행한다.
    }).detach();
// 현재 코드 블록 범위를 종료한다.
}

// 가공식품 검색 흐름에서 사용하는 함수를 선언하거나 정의한다.
std::string ProcessedFoodSearchService::trim(std::string value)
// 새 코드 블록 범위를 시작한다.
{
    // 가공식품 검색 흐름에서 사용하는 함수를 선언하거나 정의한다.
    const auto notSpace = [](unsigned char ch) { return !std::isspace(ch); };

    // 검색 워크플로우의 다음 구문을 실행한다.
    value.erase(value.begin(),
                // 가공식품 검색 흐름에서 사용하는 함수를 선언하거나 정의한다.
                std::find_if(value.begin(), value.end(), notSpace));
    // 검색 워크플로우의 다음 구문을 실행한다.
    value.erase(
        // 가공식품 검색 흐름에서 사용하는 함수를 선언하거나 정의한다.
        std::find_if(value.rbegin(), value.rend(), notSpace).base(), value.end());
    // 현재 함수에서 값을 반환한다.
    return value;
// 현재 코드 블록 범위를 종료한다.
}

// 가공식품 검색 흐름에서 사용하는 함수를 선언하거나 정의한다.
std::string ProcessedFoodSearchService::normalizeForSearch(std::string value)
// 새 코드 블록 범위를 시작한다.
{
    // 가공식품 검색 흐름에서 사용하는 함수를 선언하거나 정의한다.
    value = toLowerAscii(trim(std::move(value)));
    // 가공식품 검색 흐름에서 사용하는 함수를 선언하거나 정의한다.
    value.erase(std::remove_if(value.begin(),
                               // 검색 워크플로우의 다음 구문을 실행한다.
                               value.end(),
                               // 검색 워크플로우의 다음 구문을 실행한다.
                               [](unsigned char ch) {
                                   // 현재 함수에서 값을 반환한다.
                                   return std::isspace(ch) != 0;
                               // 검색 워크플로우의 다음 구문을 실행한다.
                               }),
                // 검색 워크플로우의 다음 구문을 실행한다.
                value.end());
    // 현재 함수에서 값을 반환한다.
    return value;
// 현재 코드 블록 범위를 종료한다.
}

// 가공식품 검색 흐름에서 사용하는 함수를 선언하거나 정의한다.
std::optional<std::string> ProcessedFoodSearchService::getEnvValue(const char *key)
// 새 코드 블록 범위를 시작한다.
{
    // 가공식품 검색 흐름에서 사용하는 함수를 선언하거나 정의한다.
    const auto *raw = std::getenv(key);
    // 조건을 검사하고 만족 시 분기한다.
    if (raw == nullptr)
    // 새 코드 블록 범위를 시작한다.
    {
        // 현재 함수에서 값을 반환한다.
        return std::nullopt;
    // 현재 코드 블록 범위를 종료한다.
    }

    // 가공식품 검색 흐름에서 사용하는 함수를 선언하거나 정의한다.
    const std::string value(raw);
    // 조건을 검사하고 만족 시 분기한다.
    if (value.empty())
    // 새 코드 블록 범위를 시작한다.
    {
        // 현재 함수에서 값을 반환한다.
        return std::nullopt;
    // 현재 코드 블록 범위를 종료한다.
    }
    // 현재 함수에서 값을 반환한다.
    return value;
// 현재 코드 블록 범위를 종료한다.
}

// 가공식품 검색 흐름에서 사용하는 함수를 선언하거나 정의한다.
std::optional<std::string> ProcessedFoodSearchService::getServiceKeyFromLocalFile()
// 새 코드 블록 범위를 시작한다.
{
    // 여러 함수 호출 간 재사용할 정적 저장소를 정의한다.
    static std::once_flag once;
    // 여러 함수 호출 간 재사용할 정적 저장소를 정의한다.
    static std::optional<std::string> cached;
    // 가공식품 검색 흐름에서 사용하는 함수를 선언하거나 정의한다.
    std::call_once(once, []() {
        // 이후 로직에서 사용할 값을 계산하거나 대입한다.
        const std::vector<std::filesystem::path> candidates = {
            // 가공식품 검색 흐름에서 사용하는 함수를 선언하거나 정의한다.
            std::filesystem::path("config") / "keys.local.json",
            // 가공식품 검색 흐름에서 사용하는 함수를 선언하거나 정의한다.
            std::filesystem::path("backend") / "config" / "keys.local.json",
            // 가공식품 검색 흐름에서 사용하는 함수를 선언하거나 정의한다.
            std::filesystem::path("..") / "config" / "keys.local.json",
            // 가공식품 검색 흐름에서 사용하는 함수를 선언하거나 정의한다.
            std::filesystem::path("..") / ".." / "config" / "keys.local.json",
            // 가공식품 검색 흐름에서 사용하는 함수를 선언하거나 정의한다.
            std::filesystem::path("..") / ".." / ".." / "config" /
                // 검색 워크플로우의 다음 구문을 실행한다.
                "keys.local.json",
        // 타입 또는 집합 정의를 닫는다.
        };

        // 컬렉션 또는 수치 범위를 순회한다.
        for (const auto &path : candidates)
        // 새 코드 블록 범위를 시작한다.
        {
            // 조건을 검사하고 만족 시 분기한다.
            if (!std::filesystem::exists(path))
            // 새 코드 블록 범위를 시작한다.
            {
                // 이번 반복의 나머지를 건너뛰고 다음 반복으로 이동한다.
                continue;
            // 현재 코드 블록 범위를 종료한다.
            }

            // 가공식품 검색 흐름에서 사용하는 함수를 선언하거나 정의한다.
            std::ifstream file(path);
            // 조건을 검사하고 만족 시 분기한다.
            if (!file.is_open())
            // 새 코드 블록 범위를 시작한다.
            {
                // 이번 반복의 나머지를 건너뛰고 다음 반복으로 이동한다.
                continue;
            // 현재 코드 블록 범위를 종료한다.
            }

            // 검색 워크플로우의 다음 구문을 실행한다.
            Json::Value root;
            // 검색 워크플로우의 다음 구문을 실행한다.
            Json::CharReaderBuilder builder;
            // 데이터 변환/관리를 위해 표준 라이브러리 유틸리티를 사용한다.
            std::string errors;
            // 조건을 검사하고 만족 시 분기한다.
            if (!Json::parseFromStream(builder, file, &root, &errors))
            // 새 코드 블록 범위를 시작한다.
            {
                // 이번 반복의 나머지를 건너뛰고 다음 반복으로 이동한다.
                continue;
            // 현재 코드 블록 범위를 종료한다.
            }

            // 조건을 검사하고 만족 시 분기한다.
            if (!root.isObject() || !root["publicNutriServiceKey"].isString())
            // 새 코드 블록 범위를 시작한다.
            {
                // 이번 반복의 나머지를 건너뛰고 다음 반복으로 이동한다.
                continue;
            // 현재 코드 블록 범위를 종료한다.
            }

            // 이후 로직에서 사용할 값을 계산하거나 대입한다.
            auto value =
                // 가공식품 검색 흐름에서 사용하는 함수를 선언하거나 정의한다.
                ProcessedFoodSearchService::trim(root["publicNutriServiceKey"].asString());
            // 조건을 검사하고 만족 시 분기한다.
            if (value.empty())
            // 새 코드 블록 범위를 시작한다.
            {
                // 이번 반복의 나머지를 건너뛰고 다음 반복으로 이동한다.
                continue;
            // 현재 코드 블록 범위를 종료한다.
            }

            // 이후 로직에서 사용할 값을 계산하거나 대입한다.
            cached = value;
            // 현재 함수에서 값을 반환한다.
            return;
        // 현재 코드 블록 범위를 종료한다.
        }
    // 검색 워크플로우의 다음 구문을 실행한다.
    });

    // 현재 함수에서 값을 반환한다.
    return cached;
// 현재 코드 블록 범위를 종료한다.
}

// 현재 코드 블록 범위를 종료한다.
}  // ingredient 네임스페이스 종료


