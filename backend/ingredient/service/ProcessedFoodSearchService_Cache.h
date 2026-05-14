/*
 * 파일 개요: 가공식품 검색 결과 캐시 컴포넌트 인터페이스/내부 엔트리 선언 파일이다.
 * 주요 역할: TTL 캐시 API(get/put)와 만료 시각 포함 저장 구조를 정의한다.
 * 사용 위치: ProcessedFoodSearchService 내부 상태로 포함되어 검색 응답 재사용 계층으로 동작한다.
 */
#pragma once

#include <chrono>
#include <mutex>
#include <optional>
#include <string>
#include <unordered_map>

#include "ingredient/model/IngredientTypes.h"

namespace ingredient
{

// 키워드 정규화 키 기준 TTL 캐시.
class ProcessedFoodSearchService_Cache
{
  public:
    // 캐시 조회(만료 시 nullopt).
    std::optional<ProcessedFoodSearchResultDTO> get(const std::string &cacheKey);
    // 캐시 저장.
    void put(const std::string &cacheKey,
             const ProcessedFoodSearchResultDTO &result);

  private:
    // 캐시 항목은 "응답 데이터 + 만료시각"을 함께 저장해 get 시 즉시 유효성 판별이 가능하도록 한다.
    struct CacheEntry
    {
        std::chrono::steady_clock::time_point expiresAt;
        ProcessedFoodSearchResultDTO result;
    };

    static constexpr std::chrono::seconds kSearchCacheTtl{120};

    // 다중 요청 스레드가 동시에 get/put을 호출할 수 있어 단일 뮤텍스로 보호한다.
    std::mutex mutex_;
    // 키워드 정규화 토큰을 키로 사용하는 in-memory 캐시 저장소.
    std::unordered_map<std::string, CacheEntry> cache_;
};

}  // namespace ingredient

