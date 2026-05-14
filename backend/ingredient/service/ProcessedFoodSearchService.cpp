/*
 * 파일 개요: 가공식품 중심 영양 검색 서비스 구현 파일이다.
 * 주요 역할: 키워드 검증, 캐시/서비스키 확인, 공공 API 조회, 연관 검색 확장, 결과 정렬/제한 후 응답을 구성한다.
 * 사용 위치: IngredientController의 가공식품 검색 API에서 직접 호출되는 비즈니스 서비스 구현체다.
 */
#include "ingredient/service/ProcessedFoodSearchService.h"

#include <thread>
#include <unordered_set>
#include <utility>

namespace ingredient
{

// 검색 흐름: 입력 검증 -> 캐시 확인 -> API 키 확인 -> 비동기 호출 -> 결과 조립.
void ProcessedFoodSearchService::searchProcessedFoods(
    const std::string &keyword,
    ProcessedFoodSearchCallback &&callback)
{
    ProcessedFoodSearchResultDTO invalidInput;
    const auto normalizedKeyword = text_.trim(keyword);
    // 빈 검색어는 외부 API를 호출하지 않고 400을 반환한다.
    if (normalizedKeyword.empty())
    {
        invalidInput.statusCode = 400;
        invalidInput.message = "Keyword is required.";
        callback(std::move(invalidInput));
        return;
    }

    const auto normalizedKeywordToken = text_.normalizeForSearch(normalizedKeyword);
    // 동일 키워드 재요청은 캐시 응답으로 즉시 처리한다.
    if (const auto cached = cache_.get(normalizedKeywordToken); cached.has_value())
    {
        callback(*cached);
        return;
    }

    const auto serviceKey = keyProvider_.getServiceKey();
    // 서비스 키가 없으면 검색 자체를 진행할 수 없다.
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

    auto *clientLayer = &client_;
    auto *assemblerLayer = &assembler_;
    auto *cacheLayer = &cache_;

    // 네트워크 I/O를 분리해 호출 스레드 블로킹을 피한다.
    std::thread([clientLayer,
                 assemblerLayer,
                 cacheLayer,
                 serviceKey = *serviceKey,
                 normalizedKeyword,
                 normalizedKeywordToken,
                 callback = std::move(callback)]() mutable {
        ProcessedFoodSearchResultDTO result;
        const auto client = clientLayer->makeClient();
        if (!client)
        {
            result.statusCode = 502;
            result.message = "Failed to call nutrition public API.";
            callback(std::move(result));
            return;
        }

        std::string errorMessage;
        constexpr std::uint64_t kPrimaryRows = 30;
        // 1차 검색: foodNm 키워드로 직접 매칭되는 후보를 우선 수집한다.
        const auto primaryPage = clientLayer->requestRowsPage(client,
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
        // 중복 foodCode 제거를 위한 집합.
        std::unordered_set<std::string> seenFoodCodes;
        seenFoodCodes.reserve(primaryPage->rows.size() + 64U);

        assemblerLayer->appendRows(mergedRows,
                                   seenFoodCodes,
                                   primaryPage->rows,
                                   false,
                                   normalizedKeywordToken);

        const auto manufacturerNames =
            assemblerLayer->collectManufacturerNames(primaryPage->rows);
        const auto foodLevel4Names =
            assemblerLayer->collectFoodLevel4Names(primaryPage->rows);

        // 연관 검색 범위를 제한해 응답 지연과 API 부하를 제어한다.
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

            // 분류명이 없으면 제조사 단독 조건으로 페이지를 순회한다.
            if (foodLevel4Names.empty())
            {
                ++processedCombinations;

                for (std::uint64_t pageNo = 1; pageNo <= kMaxRelatedPages; ++pageNo)
                {
                    std::string relatedError;
                    const auto relatedPage = clientLayer->requestRowsPage(
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

                    assemblerLayer->appendRows(mergedRows,
                                               seenFoodCodes,
                                               relatedPage->rows,
                                               true,
                                               normalizedKeywordToken);

                    if (relatedPage->rows.size() < kRelatedRowsPerPage)
                    {
                        break;
                    }
                }
                continue;
            }

            // 분류명이 있으면 제조사+분류 조합으로 확장 검색한다.
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
                    const auto relatedPage = clientLayer->requestRowsPage(
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

                    assemblerLayer->appendRows(mergedRows,
                                               seenFoodCodes,
                                               relatedPage->rows,
                                               true,
                                               normalizedKeywordToken);

                    if (relatedPage->rows.size() < kRelatedRowsPerPage)
                    {
                        break;
                    }
                }
            }
        }

        auto foods = assemblerLayer->mapRowsToFoods(mergedRows, true);
        // exact/prefix 우선순위 기준으로 정렬한다.
        assemblerLayer->sortFoods(foods, normalizedKeywordToken);

        constexpr std::size_t kMaxResultCount = 30U;
        // 응답 크기 상한을 고정해 과도한 페이로드를 방지한다.
        if (foods.size() > kMaxResultCount)
        {
            foods.resize(kMaxResultCount);
        }

        result.ok = true;
        result.statusCode = 200;
        result.message = "Nutrition foods loaded.";
        result.foods = std::move(foods);
        // 성공 결과는 캐시에 저장해 동일 검색 지연을 줄인다.
        cacheLayer->put(normalizedKeywordToken, result);
        callback(std::move(result));
    }).detach();
}

}  // namespace ingredient


