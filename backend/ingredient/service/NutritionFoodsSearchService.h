/*
 * 파일 개요: 통합 영양 검색 서비스 인터페이스 및 내부 캐시 구조 선언 파일이다.
 * 주요 역할: 검색 옵션 계약, 비동기 콜백 타입, 카테고리 통합 검색 서비스의 상태(의존성/캐시)를 정의한다.
 * 사용 위치: IngredientController가 통합 영양 검색 요청을 위임할 때 사용하는 서비스 타입이다.
 */
#pragma once

#include <chrono>
#include <cstdint>
#include <functional>
#include <mutex>
#include <optional>
#include <string>
#include <unordered_map>

#include "ingredient/model/IngredientTypes.h"
#include "ingredient/service/ProcessedFoodSearchService_Assembler.h"
#include "ingredient/service/ProcessedFoodSearchService_Client.h"
#include "ingredient/service/ProcessedFoodSearchService_KeyProvider.h"
#include "ingredient/service/ProcessedFoodSearchService_Text.h"

namespace ingredient
{

// processed/material/food 3개 카테고리를 통합 조회하는 검색 서비스.
class NutritionFoodsSearchService
{
  public:
    using NutritionFoodsSearchCallback =
        std::function<void(NutritionFoodsSearchResultDTO)>;

    struct SearchOptions
    {
        // sourceType 미지정 시 3개 카테고리를 모두 조회한다.
        std::optional<std::string> sourceType;
        // 카테고리별 페이지 번호(1-base).
        std::uint64_t page{1U};
        // 카테고리별 페이지 크기.
        std::uint64_t pageSize{5U};
    };

    NutritionFoodsSearchService() = default;

    // 키워드 + 옵션 기반 통합 검색을 수행해 콜백으로 전달한다.
    void searchFoods(const std::string &keyword,
                     const SearchOptions &options,
                     NutritionFoodsSearchCallback &&callback);

  private:
    // TTL 캐시 엔트리.
    struct CacheEntry
    {
        std::chrono::steady_clock::time_point expiresAt;
        NutritionFoodsSearchResultDTO result;
    };

    static constexpr std::chrono::seconds kSearchCacheTtl{120};

    // 캐시 조회(만료 체크 포함).
    std::optional<NutritionFoodsSearchResultDTO> getCached(
        const std::string &cacheKey);
    // 캐시 저장.
    void putCache(const std::string &cacheKey,
                  const NutritionFoodsSearchResultDTO &result);

    // 카테고리별 공공 API 페이지 호출.
    ProcessedFoodSearchService_Client client_;
    // 응답 row 병합/DTO 변환/정렬 담당.
    ProcessedFoodSearchService_Assembler assembler_;
    // 공공 API 서비스 키 조회 담당.
    ProcessedFoodSearchService_KeyProvider keyProvider_;
    // 키워드/옵션 정규화 및 파싱 유틸.
    ProcessedFoodSearchService_Text text_;

    // 캐시는 여러 요청 스레드가 공유하므로 뮤텍스로 동기화한다.
    std::mutex cacheMutex_;
    // 키 구조: normalizedKeyword|sourceType|page|pageSize
    std::unordered_map<std::string, CacheEntry> cache_;
};

}  // namespace ingredient
