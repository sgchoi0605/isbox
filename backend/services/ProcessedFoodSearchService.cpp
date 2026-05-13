// 구현 파일에서 사용하는 의존 헤더
#include "ProcessedFoodSearchService.h"

#include <drogon/drogon.h>
#include <drogon/utils/Utilities.h>

#include <algorithm>
#include <cctype>
#include <chrono>
#include <cstdlib>
#include <exception>
#include <filesystem>
#include <fstream>
#include <mutex>
#include <regex>
#include <thread>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

// 파일 내부 전용 유틸리티 네임스페이스
namespace
{

// 가공식품 검색에 사용하는 로컬 헬퍼 함수
std::optional<std::string> optionalJsonString(const Json::Value &value)
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

std::optional<std::string> firstStringFromKeys(
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

std::optional<double> optionalJsonDouble(const Json::Value &value)
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

std::optional<double> firstDoubleFromKeys(const Json::Value &row,
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

std::string toLowerAscii(std::string value)
{
    std::transform(value.begin(), value.end(), value.begin(), [](unsigned char ch) {
        return static_cast<char>(std::tolower(ch));
    });
    return value;
}

std::string trimCopy(std::string value)
{
    const auto notSpace = [](unsigned char ch) { return !std::isspace(ch); };
    value.erase(value.begin(),
                std::find_if(value.begin(), value.end(), notSpace));
    value.erase(
        std::find_if(value.rbegin(), value.rend(), notSpace).base(), value.end());
    return value;
}

std::optional<std::string> normalizeImportYnForApi(
    const std::optional<std::string> &value)
{
    if (!value.has_value())
    {
        return std::nullopt;
    }

    const auto normalized = toLowerAscii(trimCopy(*value));
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

std::optional<std::string> normalizeDataBaseDate(
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

std::optional<std::uint64_t> optionalJsonUInt64(const Json::Value &value)
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

std::optional<std::string> extractPublicApiErrorMessage(const Json::Value &body)
{
    if (!body.isMember("response") || !body["response"].isObject())
    {
        return std::nullopt;
    }

    const auto &responseRoot = body["response"];
    if (!responseRoot.isMember("header") || !responseRoot["header"].isObject())
    {
        return std::nullopt;
    }

    const auto &header = responseRoot["header"];
    const auto resultCode =
        firstStringFromKeys(header, {"resultCode", "RESULT_CODE"});
    const auto resultMsg = firstStringFromKeys(header, {"resultMsg", "RESULT_MSG"});
    if (!resultCode.has_value() || *resultCode == "00")
    {
        return std::nullopt;
    }

    return "Nutrition public API error (" + *resultCode +
           "): " + resultMsg.value_or("Unknown error.");
}

std::optional<std::uint64_t> extractTotalCount(const Json::Value &body)
{
    if (!body.isMember("response") || !body["response"].isObject())
    {
        return std::nullopt;
    }

    const auto &responseRoot = body["response"];
    if (!responseRoot.isMember("body") || !responseRoot["body"].isObject())
    {
        return std::nullopt;
    }

    const auto &bodyNode = responseRoot["body"];
    if (!bodyNode.isMember("totalCount"))
    {
        return std::nullopt;
    }

    return optionalJsonUInt64(bodyNode["totalCount"]);
}

std::vector<Json::Value> extractRows(const Json::Value &root)
{
    std::vector<Json::Value> out;

    if (root.isMember("data") && root["data"].isArray())
    {
        for (const auto &item : root["data"])
        {
            out.push_back(item);
        }
        return out;
    }

    if (root.isMember("response") && root["response"].isObject())
    {
        const auto &response = root["response"];
        if (response.isMember("body") && response["body"].isObject())
        {
            const auto &body = response["body"];
            if (body.isMember("items"))
            {
                const auto &items = body["items"];
                if (items.isArray())
                {
                    for (const auto &item : items)
                    {
                        out.push_back(item);
                    }
                    return out;
                }

                if (items.isObject() && items.isMember("item"))
                {
                    const auto &itemField = items["item"];
                    if (itemField.isArray())
                    {
                        for (const auto &item : itemField)
                        {
                            out.push_back(item);
                        }
                        return out;
                    }

                    if (itemField.isObject())
                    {
                        out.push_back(itemField);
                        return out;
                    }
                }
            }
        }
    }

    return out;
}

bool looksLikePercentEncoded(const std::string &value)
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

void appendEncodedQueryParam(std::string &path,
                             const char *key,
                             const std::optional<std::string> &value)
{
    if (!value.has_value())
    {
        return;
    }

    const auto trimmed = trimCopy(*value);
    if (trimmed.empty())
    {
        return;
    }

    const auto encoded = looksLikePercentEncoded(trimmed)
                             ? trimmed
                             : drogon::utils::urlEncodeComponent(trimmed);
    path += "&";
    path += key;
    path += "=";
    path += encoded;
}

std::string buildPublicNutritionApiPath(
    const std::string &serviceKey,
    std::uint64_t pageNo,
    std::uint64_t numOfRows,
    const std::optional<std::string> &foodKeyword,
    const std::optional<std::string> &manufacturerName = std::nullopt,
    const std::optional<std::string> &foodLevel4Name = std::nullopt)
{
    std::string path =
        "/openapi/tn_pubr_public_nutri_process_info_api"
        "?serviceKey=" +
        drogon::utils::urlEncodeComponent(serviceKey) + "&type=json&pageNo=" +
        std::to_string(pageNo) + "&numOfRows=" + std::to_string(numOfRows);

    appendEncodedQueryParam(path, "foodNm", foodKeyword);
    appendEncodedQueryParam(path, "mfrNm", manufacturerName);
    appendEncodedQueryParam(path, "foodLv4Nm", foodLevel4Name);

    return path;
}

drogon::HttpClientPtr makePublicNutritionApiClient()
{
    // 일부 윈도우 환경에서는 드로곤 비동기 리졸버의 호스트 이름 해석이 실패할 수 있다.
    return drogon::HttpClient::newHttpClient("https://27.101.215.193",
                                             nullptr,
                                             false,
                                             false);
}

struct PublicNutritionRowsPage
{
    std::vector<Json::Value> rows;
    std::optional<std::uint64_t> totalCount;
};

std::optional<PublicNutritionRowsPage> requestPublicNutritionRowsPage(
    const drogon::HttpClientPtr &client,
    const std::string &serviceKey,
    std::uint64_t pageNo,
    std::uint64_t numOfRows,
    const std::optional<std::string> &foodKeyword,
    const std::optional<std::string> &manufacturerName,
    const std::optional<std::string> &foodLevel4Name,
    std::string &errorMessage)
{
    auto request = drogon::HttpRequest::newHttpRequest();
    request->setMethod(drogon::Get);
    request->setPathEncode(false);
    request->setPath(buildPublicNutritionApiPath(serviceKey,
                                                 pageNo,
                                                 numOfRows,
                                                 foodKeyword,
                                                 manufacturerName,
                                                 foodLevel4Name));
    request->addHeader("Host", "api.data.go.kr");

    const auto [reqResult, response] = client->sendRequest(request, 20.0);
    if (reqResult != drogon::ReqResult::Ok || !response)
    {
        errorMessage = "Failed to call nutrition public API. (ReqResult: " +
                       drogon::to_string(reqResult) + ")";
        return std::nullopt;
    }

    if (response->statusCode() != drogon::k200OK)
    {
        errorMessage = "Nutrition public API returned error.";
        return std::nullopt;
    }

    const auto body = response->getJsonObject();
    if (!body)
    {
        errorMessage = "Invalid response from nutrition public API.";
        return std::nullopt;
    }

    if (const auto apiError = extractPublicApiErrorMessage(*body); apiError.has_value())
    {
        if (apiError->find("Nutrition public API error (03)") != std::string::npos)
        {
            PublicNutritionRowsPage emptyPage;
            emptyPage.totalCount = 0U;
            return emptyPage;
        }

        errorMessage = *apiError;
        return std::nullopt;
    }

    PublicNutritionRowsPage page;
    page.rows = extractRows(*body);
    page.totalCount = extractTotalCount(*body);
    return page;
}

std::vector<ingredient::ProcessedFoodSearchItemDTO> mapRowsToFoods(
    const std::vector<Json::Value> &rows,
    bool dedupeByFoodName)
{
    std::vector<ingredient::ProcessedFoodSearchItemDTO> foods;
    foods.reserve(rows.size());

    std::unordered_set<std::string> seenFoodNames;
    if (dedupeByFoodName)
    {
        seenFoodNames.reserve(rows.size());
    }

    for (const auto &row : rows)
    {
        auto foodCode = firstStringFromKeys(row, {"foodCd", "FOOD_CD"});
        auto foodName = firstStringFromKeys(row, {"foodNm", "FOOD_NM"});
        if (!foodCode.has_value() || !foodName.has_value())
        {
            continue;
        }

        if (dedupeByFoodName && !seenFoodNames.insert(*foodName).second)
        {
            continue;
        }

        ingredient::ProcessedFoodSearchItemDTO dto;
        dto.foodCode = *foodCode;
        dto.foodName = *foodName;
        dto.foodGroupName = firstStringFromKeys(
            row,
            {"foodLv6Nm", "foodLv5Nm", "foodLv4Nm", "foodLv3Nm",
             "foodLv7Nm", "FOOD_LV6_NM", "FOOD_LV5_NM", "FOOD_LV4_NM",
             "FOOD_LV3_NM", "FOOD_LV7_NM"});
        dto.nutritionBasisAmount =
            firstStringFromKeys(row, {"nutConSrtrQua", "NUT_CON_SRTR_QUA"});
        dto.energyKcal = firstDoubleFromKeys(row, {"enerc", "ENERC"});
        dto.proteinG = firstDoubleFromKeys(row, {"prot", "PROT"});
        dto.fatG = firstDoubleFromKeys(row, {"fatce", "FATCE"});
        dto.carbohydrateG = firstDoubleFromKeys(row, {"chocdf", "CHOCDF"});
        dto.sugarG = firstDoubleFromKeys(row, {"sugar", "SUGAR"});
        dto.sodiumMg = firstDoubleFromKeys(row, {"nat", "NAT"});
        dto.sourceName = firstStringFromKeys(row, {"srcNm", "SRC_NM"});
        dto.manufacturerName = firstStringFromKeys(row, {"mfrNm", "MFR_NM"});
        dto.importYn =
            normalizeImportYnForApi(firstStringFromKeys(row, {"imptYn", "IMPT_YN"}));
        dto.originCountryName = firstStringFromKeys(row, {"cooNm", "COO_NM"});
        dto.dataBaseDate =
            normalizeDataBaseDate(firstStringFromKeys(row, {"crtrYmd", "CRTR_YMD"}));
        foods.push_back(std::move(dto));
    }

    return foods;
}

int searchPriority(const ingredient::ProcessedFoodSearchItemDTO &food,
                   const std::string &normalizedKeywordToken)
{
    if (normalizedKeywordToken.empty())
    {
        return 2;
    }

    auto normalizedFoodName = trimCopy(food.foodName);
    normalizedFoodName = toLowerAscii(std::move(normalizedFoodName));
    normalizedFoodName.erase(
        std::remove_if(normalizedFoodName.begin(),
                       normalizedFoodName.end(),
                       [](unsigned char ch) {
                           return std::isspace(ch) != 0;
                       }),
        normalizedFoodName.end());

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

struct CacheEntry
{
    std::chrono::steady_clock::time_point expiresAt;
    ingredient::ProcessedFoodSearchResultDTO result;
};

// 검색 흐름 상수
constexpr std::chrono::seconds kSearchCacheTtl(120);
std::mutex gSearchCacheMutex;
std::unordered_map<std::string, CacheEntry> gSearchCache;

std::optional<ingredient::ProcessedFoodSearchResultDTO> getCachedSearchResult(
    const std::string &normalizedKeyword)
{
    std::lock_guard<std::mutex> lock(gSearchCacheMutex);
    const auto it = gSearchCache.find(normalizedKeyword);
    if (it == gSearchCache.end())
    {
        return std::nullopt;
    }

    if (std::chrono::steady_clock::now() >= it->second.expiresAt)
    {
        gSearchCache.erase(it);
        return std::nullopt;
    }

    return it->second.result;
}

void cacheSearchResult(const std::string &normalizedKeyword,
                       const ingredient::ProcessedFoodSearchResultDTO &result)
{
    std::lock_guard<std::mutex> lock(gSearchCacheMutex);
    gSearchCache[normalizedKeyword] = CacheEntry{
        std::chrono::steady_clock::now() + kSearchCacheTtl,
        result,
    };
}

}  // 익명 네임스페이스 종료

// 서비스 구현 네임스페이스
namespace ingredient
{

// 공공 API 조회, 결과 병합, 정렬을 수행하는 메인 검색 흐름
void ProcessedFoodSearchService::searchProcessedFoods(
    const std::string &keyword,
    ProcessedFoodSearchCallback &&callback)
{
    ProcessedFoodSearchResultDTO invalidInput;
    const auto normalizedKeyword = trim(keyword);
    if (normalizedKeyword.empty())
    {
        invalidInput.statusCode = 400;
        invalidInput.message = "Keyword is required.";
        callback(std::move(invalidInput));
        return;
    }

    const auto normalizedKeywordToken = normalizeForSearch(normalizedKeyword);
    if (const auto cached = getCachedSearchResult(normalizedKeywordToken);
        cached.has_value())
    {
        callback(*cached);
        return;
    }

    auto serviceKey = getEnvValue("PUBLIC_NUTRI_SERVICE_KEY");
    if (!serviceKey.has_value())
    {
        serviceKey = getServiceKeyFromLocalFile();
    }

    if (!serviceKey.has_value())
    {
        ProcessedFoodSearchResultDTO keyError;
        keyError.statusCode = 500;
        keyError.message =
            "PUBLIC_NUTRI_SERVICE_KEY is not configured. "
            "Set env var or backend/config/keys.local.json.";
        callback(std::move(keyError));
        return;
    }

    std::thread([serviceKey = *serviceKey,
                 normalizedKeyword,
                 normalizedKeywordToken,
                 callback = std::move(callback)]() mutable {
        ProcessedFoodSearchResultDTO result;
        const auto client = makePublicNutritionApiClient();
        if (!client)
        {
            result.statusCode = 502;
            result.message = "Failed to call nutrition public API.";
            callback(std::move(result));
            return;
        }

        std::string errorMessage;
// 검색 흐름 상수
        constexpr std::uint64_t kPrimaryRows = 30;
        const auto primaryPage = requestPublicNutritionRowsPage(client,
                                                                serviceKey,
                                                                1,
                                                                kPrimaryRows,
                                                                normalizedKeyword,
                                                                std::nullopt,
                                                                std::nullopt,
                                                                errorMessage);
        if (!primaryPage.has_value())
        {
            result.statusCode = 502;
            result.message =
                errorMessage.empty() ? "Failed to call nutrition public API."
                                     : errorMessage;
            callback(std::move(result));
            return;
        }

        std::vector<Json::Value> mergedRows;
        mergedRows.reserve(primaryPage->rows.size() + 64U);

        std::unordered_set<std::string> seenFoodCodes;
        seenFoodCodes.reserve(primaryPage->rows.size() + 64U);

        auto appendRows = [&](const std::vector<Json::Value> &rows,
                              bool filterByKeyword) {
            for (const auto &row : rows)
            {
                const auto foodCode = firstStringFromKeys(row, {"foodCd", "FOOD_CD"});
                const auto foodName = firstStringFromKeys(row, {"foodNm", "FOOD_NM"});
                if (!foodCode.has_value() || !foodName.has_value())
                {
                    continue;
                }

                if (filterByKeyword)
                {
                    auto normalizedFoodName = trimCopy(*foodName);
                    normalizedFoodName = toLowerAscii(std::move(normalizedFoodName));
                    normalizedFoodName.erase(
                        std::remove_if(normalizedFoodName.begin(),
                                       normalizedFoodName.end(),
                                       [](unsigned char ch) {
                                           return std::isspace(ch) != 0;
                                       }),
                        normalizedFoodName.end());
                    if (normalizedFoodName.find(normalizedKeywordToken) ==
                        std::string::npos)
                    {
                        continue;
                    }
                }

                if (!seenFoodCodes.insert(*foodCode).second)
                {
                    continue;
                }

                mergedRows.push_back(row);
            }
        };

        appendRows(primaryPage->rows, false);

        std::unordered_set<std::string> manufacturerNames;
        std::unordered_set<std::string> foodLevel4Names;
        for (const auto &row : primaryPage->rows)
        {
            if (const auto manufacturerName =
                    firstStringFromKeys(row, {"mfrNm", "MFR_NM"});
                manufacturerName.has_value())
            {
                const auto normalized = trimCopy(*manufacturerName);
                if (!normalized.empty())
                {
                    manufacturerNames.insert(normalized);
                }
            }

            if (const auto foodLevel4Name =
                    firstStringFromKeys(row, {"foodLv4Nm", "FOOD_LV4_NM"});
                foodLevel4Name.has_value())
            {
                const auto normalized = trimCopy(*foodLevel4Name);
                if (!normalized.empty())
                {
                    foodLevel4Names.insert(normalized);
                }
            }
        }

// 검색 흐름 상수
        constexpr std::size_t kMaxRelatedCombinations = 3U;
        constexpr std::uint64_t kRelatedRowsPerPage = 100U;
        constexpr std::uint64_t kMaxRelatedPages = 8U;
        std::size_t processedCombinations = 0U;

        for (const auto &manufacturerName : manufacturerNames)
        {
            if (processedCombinations >= kMaxRelatedCombinations)
            {
                break;
            }

            if (foodLevel4Names.empty())
            {
                ++processedCombinations;

                for (std::uint64_t pageNo = 1; pageNo <= kMaxRelatedPages; ++pageNo)
                {
                    std::string relatedError;
                    const auto relatedPage = requestPublicNutritionRowsPage(
                        client,
                        serviceKey,
                        pageNo,
                        kRelatedRowsPerPage,
                        std::nullopt,
                        manufacturerName,
                        std::nullopt,
                        relatedError);
                    if (!relatedPage.has_value())
                    {
                        break;
                    }

                    appendRows(relatedPage->rows, true);

                    if (relatedPage->rows.size() < kRelatedRowsPerPage)
                    {
                        break;
                    }
                }
                continue;
            }

            for (const auto &foodLevel4Name : foodLevel4Names)
            {
                if (processedCombinations >= kMaxRelatedCombinations)
                {
                    break;
                }
                ++processedCombinations;

                for (std::uint64_t pageNo = 1; pageNo <= kMaxRelatedPages; ++pageNo)
                {
                    std::string relatedError;
                    const auto relatedPage = requestPublicNutritionRowsPage(
                        client,
                        serviceKey,
                        pageNo,
                        kRelatedRowsPerPage,
                        std::nullopt,
                        manufacturerName,
                        foodLevel4Name,
                        relatedError);
                    if (!relatedPage.has_value())
                    {
                        break;
                    }

                    appendRows(relatedPage->rows, true);

                    if (relatedPage->rows.size() < kRelatedRowsPerPage)
                    {
                        break;
                    }
                }
            }
        }

        auto foods = mapRowsToFoods(mergedRows, true);
        std::sort(foods.begin(),
                  foods.end(),
                  [&normalizedKeywordToken](const ProcessedFoodSearchItemDTO &lhs,
                                            const ProcessedFoodSearchItemDTO &rhs) {
                      const auto lhsPriority = searchPriority(lhs, normalizedKeywordToken);
                      const auto rhsPriority = searchPriority(rhs, normalizedKeywordToken);
                      if (lhsPriority != rhsPriority)
                      {
                          return lhsPriority < rhsPriority;
                      }
                      return lhs.foodName < rhs.foodName;
                  });

// 검색 흐름 상수
        constexpr std::size_t kMaxResultCount = 30U;
        if (foods.size() > kMaxResultCount)
        {
            foods.resize(kMaxResultCount);
        }

        result.ok = true;
        result.statusCode = 200;
        result.message = "Nutrition foods loaded.";
        result.foods = std::move(foods);
        cacheSearchResult(normalizedKeywordToken, result);

        callback(std::move(result));
    }).detach();
}

// 서비스 보조 유틸리티 메서드
std::string ProcessedFoodSearchService::trim(std::string value)
{
    const auto notSpace = [](unsigned char ch) { return !std::isspace(ch); };

    value.erase(value.begin(),
                std::find_if(value.begin(), value.end(), notSpace));
    value.erase(
        std::find_if(value.rbegin(), value.rend(), notSpace).base(), value.end());
    return value;
}

std::string ProcessedFoodSearchService::normalizeForSearch(std::string value)
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

std::optional<std::string> ProcessedFoodSearchService::getEnvValue(const char *key)
{
    const auto *raw = std::getenv(key);
    if (raw == nullptr)
    {
        return std::nullopt;
    }

    const std::string value(raw);
    if (value.empty())
    {
        return std::nullopt;
    }
    return value;
}

std::optional<std::string> ProcessedFoodSearchService::getServiceKeyFromLocalFile()
{
    static std::once_flag once;
    static std::optional<std::string> cached;
    std::call_once(once, []() {
        const std::vector<std::filesystem::path> candidates = {
            std::filesystem::path("config") / "keys.local.json",
            std::filesystem::path("backend") / "config" / "keys.local.json",
            std::filesystem::path("..") / "config" / "keys.local.json",
            std::filesystem::path("..") / ".." / "config" / "keys.local.json",
            std::filesystem::path("..") / ".." / ".." / "config" /
                "keys.local.json",
        };

        for (const auto &path : candidates)
        {
            if (!std::filesystem::exists(path))
            {
                continue;
            }

            std::ifstream file(path);
            if (!file.is_open())
            {
                continue;
            }

            Json::Value root;
            Json::CharReaderBuilder builder;
            std::string errors;
            if (!Json::parseFromStream(builder, file, &root, &errors))
            {
                continue;
            }

            if (!root.isObject() || !root["publicNutriServiceKey"].isString())
            {
                continue;
            }

            auto value =
                ProcessedFoodSearchService::trim(root["publicNutriServiceKey"].asString());
            if (value.empty())
            {
                continue;
            }

            cached = value;
            return;
        }
    });

    return cached;
}

}  // ingredient 네임스페이스 종료

