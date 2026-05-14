/*
 * 파일 개요: 통합 영양 검색(가공식품/원재료/음식) 오케스트레이션 구현 파일이다.
 * 주요 역할: 입력 검증, 카테고리 병렬 수집, 부분 실패 허용 집계, 페이지 슬라이싱, TTL 캐시 저장까지 검색 파이프라인을 수행한다.
 * 사용 위치: IngredientController의 통합 영양 검색 API에서 핵심 비즈니스 로직으로 호출된다.
 */
#include "ingredient/service/NutritionFoodsSearchService.h"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <future>
#include <optional>
#include <string>
#include <thread>
#include <tuple>
#include <unordered_set>
#include <utility>
#include <vector>

namespace
{

using ingredient::NutritionFoodsSearchCountsDTO;
using ingredient::NutritionFoodsSearchGroupsDTO;
using ingredient::NutritionFoodsSearchHasMoreDTO;
using ingredient::ProcessedFoodSearchItemDTO;
using ingredient::ProcessedFoodSearchService_Client;

using FoodsGroupMember = std::vector<ProcessedFoodSearchItemDTO>
    NutritionFoodsSearchGroupsDTO::*;
using FoodsCountMember = std::uint64_t NutritionFoodsSearchCountsDTO::*;
using FoodsHasMoreMember = bool NutritionFoodsSearchHasMoreDTO::*;

struct CategorySpec
{
    ProcessedFoodSearchService_Client::CategoryEndpoint endpoint;
    const char *sourceType;
    const char *sourceTypeLabel;
    FoodsGroupMember groupsMember;
    FoodsCountMember countsMember;
    FoodsHasMoreMember hasMoreMember;
};

constexpr std::uint64_t kRowsPerCategory = 30U;
constexpr std::uint64_t kDefaultPageSize = 5U;
constexpr std::uint64_t kMaxPageSize = 10U;
constexpr std::uint64_t kFoodNameRowsPerPage = 100U;
constexpr std::uint64_t kFoodNameMaxPages = 5U;
constexpr std::uint64_t kFoodNameScanRowsPerPage = 200U;
constexpr std::uint64_t kFoodNameScanMaxPages = 12U;
constexpr std::uint64_t kFoodNameScanBatchSize = 4U;

constexpr std::array<CategorySpec, 3> kCategorySpecs = {{
    {ProcessedFoodSearchService_Client::CategoryEndpoint::Processed,
     "processed",
     "가공식품",
     &NutritionFoodsSearchGroupsDTO::processed,
     &NutritionFoodsSearchCountsDTO::processed,
     &NutritionFoodsSearchHasMoreDTO::processed},
    {ProcessedFoodSearchService_Client::CategoryEndpoint::Material,
     "material",
     "원재료",
     &NutritionFoodsSearchGroupsDTO::material,
     &NutritionFoodsSearchCountsDTO::material,
     &NutritionFoodsSearchHasMoreDTO::material},
    {ProcessedFoodSearchService_Client::CategoryEndpoint::Food,
     "food",
     "음식",
     &NutritionFoodsSearchGroupsDTO::food,
     &NutritionFoodsSearchCountsDTO::food,
     &NutritionFoodsSearchHasMoreDTO::food},
}};

}  // namespace

namespace ingredient
{

// 캐시 조회 시 만료 여부를 함께 검사한다.
std::optional<NutritionFoodsSearchResultDTO> NutritionFoodsSearchService::getCached(
    const std::string &cacheKey)
{
    std::lock_guard<std::mutex> lock(cacheMutex_);
    const auto it = cache_.find(cacheKey);
    if (it == cache_.end())
    {
        return std::nullopt;
    }

    if (std::chrono::steady_clock::now() >= it->second.expiresAt)
    {
        cache_.erase(it);
        return std::nullopt;
    }

    return it->second.result;
}

// 완성된 검색 결과를 TTL과 함께 캐시에 저장한다.
void NutritionFoodsSearchService::putCache(
    const std::string &cacheKey,
    const NutritionFoodsSearchResultDTO &result)
{
    std::lock_guard<std::mutex> lock(cacheMutex_);
    cache_[cacheKey] = CacheEntry{
        std::chrono::steady_clock::now() + kSearchCacheTtl,
        result,
    };
}

// 통합 검색 흐름:
// 1) 입력/옵션 검증 2) 캐시 확인 3) API 키 확인 4) 카테고리별 비동기 수집 5) 페이징 응답 조립
void NutritionFoodsSearchService::searchFoods(
    const std::string &keyword,
    const SearchOptions &options,
    NutritionFoodsSearchCallback &&callback)
{
    NutritionFoodsSearchResultDTO invalidInput;
    const auto normalizedKeyword = text_.trim(keyword);
    // 빈 키워드는 즉시 400 반환.
    if (normalizedKeyword.empty())
    {
        invalidInput.statusCode = 400;
        invalidInput.message = "Keyword is required.";
        callback(std::move(invalidInput));
        return;
    }

    const auto page = options.page == 0U ? 1U : options.page;
    const auto pageSize = (std::min)(kMaxPageSize,
                                     options.pageSize == 0U ? kDefaultPageSize
                                                             : options.pageSize);
    std::optional<std::string> normalizedSourceType;
    // sourceType이 있으면 허용값(processed/material/food)인지 검증한다.
    if (options.sourceType.has_value())
    {
        const auto trimmedSourceType = text_.trim(*options.sourceType);
        if (!trimmedSourceType.empty())
        {
            normalizedSourceType = text_.toLowerAscii(trimmedSourceType);
            if (*normalizedSourceType != "processed" &&
                *normalizedSourceType != "material" &&
                *normalizedSourceType != "food")
            {
                invalidInput.statusCode = 400;
                invalidInput.message = "Invalid sourceType. Use processed/material/food.";
                callback(std::move(invalidInput));
                return;
            }
        }
    }

    const auto normalizedKeywordToken = text_.normalizeForSearch(normalizedKeyword);
    const auto cacheKey = normalizedKeywordToken + "|" +
                          normalizedSourceType.value_or("all") + "|" +
                          std::to_string(page) + "|" + std::to_string(pageSize);
    // 키워드/옵션 단위 캐시 hit 시 즉시 반환한다.
    if (const auto cached = getCached(cacheKey); cached.has_value())
    {
        callback(*cached);
        return;
    }

    const auto serviceKey = keyProvider_.getServiceKey();
    // 서비스 키 미설정 시 외부 API 호출 불가.
    if (!serviceKey.has_value())
    {
        NutritionFoodsSearchResultDTO keyError;
        keyError.statusCode = 500;
        keyError.message =
            "PUBLIC_NUTRI_SERVICE_KEY is not configured. "
            "Set env var or backend/config/keys.local.json.";
        callback(std::move(keyError));
        return;
    }

    auto *clientLayer = &client_;
    auto *assemblerLayer = &assembler_;
    auto *cacheLayer = this;

    // 네트워크 중심 작업은 별도 스레드에서 수행한다.
    std::thread([clientLayer,
                 assemblerLayer,
                 cacheLayer,
                 serviceKey = *serviceKey,
                 normalizedKeywordToken,
                 normalizedKeyword,
                 normalizedSourceType,
                 page,
                 pageSize,
                 cacheKey,
                 callback = std::move(callback)]() mutable {
        NutritionFoodsSearchResultDTO result;
        struct CategoryCollectResult
        {
            const CategorySpec *category{nullptr};
            bool requestSucceeded{false};
            bool requestFailed{false};
            std::string firstErrorMessage;
            std::vector<ProcessedFoodSearchItemDTO> foods;
            bool hasMore{false};
        };

        bool hasSuccessfulCategory = false;
        const auto offset = (page - 1U) * pageSize;
        const auto requestedEnd = offset + pageSize;
        // 페이지 계산상 실제로 필요한 최대 row 수를 제한한다.
        const auto neededRows = (std::min)(kRowsPerCategory, requestedEnd + 1U);

        std::vector<const CategorySpec *> selectedCategories;
        selectedCategories.reserve(kCategorySpecs.size());
        // sourceType 필터가 있으면 해당 카테고리만 선택한다.
        for (const auto &spec : kCategorySpecs)
        {
            if (!normalizedSourceType.has_value() ||
                *normalizedSourceType == spec.sourceType)
            {
                selectedCategories.push_back(&spec);
            }
        }

        std::vector<std::future<CategoryCollectResult>> categoryFutures;
        categoryFutures.reserve(selectedCategories.size());
        // 카테고리 간 요청을 병렬화해 통합 검색 응답 지연을 줄인다.
        for (const auto *categoryPtr : selectedCategories)
        {
            categoryFutures.push_back(std::async(
                std::launch::async,
                [clientLayer,
                 assemblerLayer,
                 serviceKey,
                 normalizedKeywordToken,
                 normalizedKeyword,
                 offset,
                 pageSize,
                 neededRows,
                 categoryPtr]() -> CategoryCollectResult {
                    CategoryCollectResult categoryResult;
                    categoryResult.category = categoryPtr;
                    const auto &category = *categoryPtr;

                    // 요청 페이지 범위가 카테고리 상한을 넘으면 빈 결과를 성공으로 처리한다.
                    if (offset >= kRowsPerCategory)
                    {
                        categoryResult.requestSucceeded = true;
                        return categoryResult;
                    }

                    const auto client = clientLayer->makeClient();
                    if (!client)
                    {
                        categoryResult.requestFailed = true;
                        categoryResult.firstErrorMessage =
                            "Failed to call nutrition public API.";
                        return categoryResult;
                    }

                    std::vector<Json::Value> mergedRows;
                    mergedRows.reserve((std::min)(kRowsPerCategory, neededRows) +
                                       kFoodNameRowsPerPage);
                    std::unordered_set<std::string> seenFoodCodes;
                    seenFoodCodes.reserve((std::min)(kRowsPerCategory, neededRows) +
                                          kFoodNameRowsPerPage);

                    bool categoryRequestSucceeded = false;
                    bool categoryRequestFailed = false;
                    std::string categoryFirstErrorMessage;

                    auto requestAndAppend =
                        [&](std::uint64_t pageNo,
                            std::uint64_t numOfRows,
                            const std::optional<std::string> &foodKeyword)
                        -> std::optional<std::size_t> {
                        // 공통 요청 래퍼: 실패 메시지를 누적하고 성공 row는 중복 제거 병합한다.
                        std::string errorMessage;
                        const auto rowsPage = clientLayer->requestRowsPage(
                            client,
                            serviceKey,
                            pageNo,
                            numOfRows,
                            foodKeyword,
                            std::nullopt,
                            std::nullopt,
                            errorMessage,
                            category.endpoint,
                            std::nullopt);
                        if (!rowsPage.has_value())
                        {
                            categoryRequestFailed = true;
                            if (categoryFirstErrorMessage.empty())
                            {
                                categoryFirstErrorMessage = errorMessage;
                            }
                            return std::nullopt;
                        }

                        categoryRequestSucceeded = true;
                        assemblerLayer->appendRows(mergedRows,
                                                   seenFoodCodes,
                                                   rowsPage->rows,
                                                   true,
                                                   normalizedKeywordToken);
                        return rowsPage->rows.size();
                    };

                    // 1차 검색: foodNm=keyword 페이지를 순차 수집한다.
                    for (std::uint64_t pageNo = 1U; pageNo <= kFoodNameMaxPages; ++pageNo)
                    {
                        if (mergedRows.size() >= neededRows)
                        {
                            break;
                        }

                        const auto rowsCount = requestAndAppend(pageNo,
                                                                kFoodNameRowsPerPage,
                                                                normalizedKeyword);
                        if (!rowsCount.has_value())
                        {
                            break;
                        }
                        if (*rowsCount < kFoodNameRowsPerPage)
                        {
                            break;
                        }
                    }

                    if (mergedRows.empty() && normalizedKeywordToken.size() <= 9U)
                    {
                        // 1차 결과가 없고 키워드가 짧으면 폭넓은 스캔으로 후보를 보강한다.
                        bool shouldStopScan = false;
                        for (std::uint64_t batchStart = 1U;
                             batchStart <= kFoodNameScanMaxPages && !shouldStopScan;
                             batchStart += kFoodNameScanBatchSize)
                        {
                            if (mergedRows.size() >= neededRows)
                            {
                                break;
                            }

                            const auto batchEnd =
                                (std::min)(kFoodNameScanMaxPages,
                                           batchStart + kFoodNameScanBatchSize - 1U);

                            using ScanPageResult = std::tuple<
                                std::uint64_t,
                                std::optional<ProcessedFoodSearchService_Client::RowsPage>,
                                std::string>;
                            std::vector<std::future<ScanPageResult>> futures;
                            futures.reserve(static_cast<std::size_t>(
                                batchEnd - batchStart + 1U));

                            // 스캔 페이지는 병렬 호출해 대기 시간을 줄인다.
                            for (std::uint64_t pageNo = batchStart; pageNo <= batchEnd;
                                 ++pageNo)
                            {
                                futures.push_back(std::async(
                                    std::launch::async,
                                    [clientLayer,
                                     serviceKey,
                                     pageNo,
                                     category]() -> ScanPageResult {
                                        std::string errorMessage;
                                        const auto scanClient =
                                            clientLayer->makeClient();
                                        if (!scanClient)
                                        {
                                            return {pageNo,
                                                    std::nullopt,
                                                    "Failed to call nutrition public API."};
                                        }

                                        const auto rowsPage =
                                            clientLayer->requestRowsPage(
                                                scanClient,
                                                serviceKey,
                                                pageNo,
                                                kFoodNameScanRowsPerPage,
                                                std::nullopt,
                                                std::nullopt,
                                                std::nullopt,
                                                errorMessage,
                                                category.endpoint,
                                                std::nullopt);
                                        return {pageNo, rowsPage, errorMessage};
                                    }));
                            }

                            std::vector<ScanPageResult> batchResults;
                            batchResults.reserve(futures.size());
                            for (auto &future : futures)
                            {
                                batchResults.push_back(future.get());
                            }
                            // 병렬 결과를 페이지 순서로 재정렬해 처리 순서를 안정화한다.
                            std::sort(batchResults.begin(),
                                      batchResults.end(),
                                      [](const ScanPageResult &lhs,
                                         const ScanPageResult &rhs) {
                                          return std::get<0>(lhs) < std::get<0>(rhs);
                                      });

                            for (const auto &pageResult : batchResults)
                            {
                                if (mergedRows.size() >= neededRows)
                                {
                                    shouldStopScan = true;
                                    break;
                                }

                                const auto &rowsPage = std::get<1>(pageResult);
                                const auto &errorMessage = std::get<2>(pageResult);
                                if (!rowsPage.has_value())
                                {
                                    categoryRequestFailed = true;
                                    if (categoryFirstErrorMessage.empty())
                                    {
                                        categoryFirstErrorMessage = errorMessage;
                                    }
                                    continue;
                                }

                                categoryRequestSucceeded = true;
                                assemblerLayer->appendRows(mergedRows,
                                                           seenFoodCodes,
                                                           rowsPage->rows,
                                                           true,
                                                           normalizedKeywordToken);
                                if (rowsPage->rows.size() < kFoodNameScanRowsPerPage)
                                {
                                    shouldStopScan = true;
                                    break;
                                }
                            }
                        }
                    }

                    if (!categoryRequestSucceeded && categoryRequestFailed)
                    {
                        categoryResult.requestFailed = true;
                        categoryResult.firstErrorMessage = categoryFirstErrorMessage;
                        return categoryResult;
                    }

                    if (categoryRequestSucceeded)
                    {
                        categoryResult.requestSucceeded = true;
                    }

                    // 병합 row를 DTO로 변환하고 검색 우선순위로 정렬한다.
                    auto foods = assemblerLayer->mapRowsToFoods(mergedRows, false);
                    assemblerLayer->sortFoods(foods, normalizedKeywordToken);
                    if (foods.size() > kRowsPerCategory)
                    {
                        foods.resize(kRowsPerCategory);
                    }

                    for (auto &food : foods)
                    {
                        food.sourceType = category.sourceType;
                        food.sourceTypeLabel = category.sourceTypeLabel;
                    }

                    const auto safeOffset = (std::min)(static_cast<std::size_t>(offset),
                                                       foods.size());
                    const auto safeEnd =
                        (std::min)(foods.size(),
                                   safeOffset + static_cast<std::size_t>(pageSize));

                    categoryResult.foods.assign(
                        foods.begin() + static_cast<std::ptrdiff_t>(safeOffset),
                        foods.begin() + static_cast<std::ptrdiff_t>(safeEnd));
                    categoryResult.hasMore = foods.size() > safeEnd;
                    return categoryResult;
                }));
        }

        for (auto &categoryFuture : categoryFutures)
        {
            auto categoryResult = categoryFuture.get();
            if (categoryResult.category == nullptr)
            {
                continue;
            }
            const auto &category = *categoryResult.category;

            if (!categoryResult.requestSucceeded && categoryResult.requestFailed)
            {
                // 카테고리 단위 실패는 경고로 누적하고 다른 카테고리는 계속 진행한다.
                result.failedCategories.emplace_back(category.sourceType);
                result.warnings.push_back(
                    std::string(category.sourceTypeLabel) +
                    " category failed: " +
                    (categoryResult.firstErrorMessage.empty()
                         ? std::string("Failed to call nutrition public API.")
                         : categoryResult.firstErrorMessage));
                continue;
            }

            if (categoryResult.requestSucceeded)
            {
                hasSuccessfulCategory = true;
            }

            auto &group = result.groups.*(category.groupsMember);
            group = std::move(categoryResult.foods);
            result.counts.*(category.countsMember) =
                static_cast<std::uint64_t>(group.size());
            result.hasMore.*(category.hasMoreMember) = categoryResult.hasMore;
        }

        // 모든 카테고리가 실패한 경우에만 전체 요청을 실패 처리한다.
        if (!hasSuccessfulCategory)
        {
            result.statusCode = 502;
            result.message = "Failed to call nutrition public API.";
            callback(std::move(result));
            return;
        }

        result.ok = true;
        result.statusCode = 200;
        result.message = result.failedCategories.empty()
                             ? "Nutrition foods loaded."
                             : "Nutrition foods loaded with partial failures.";
        // 완전 성공 응답만 캐시에 저장한다.
        if (result.failedCategories.empty())
        {
            cacheLayer->putCache(cacheKey, result);
        }
        callback(std::move(result));
    }).detach();
}

}  // namespace ingredient
