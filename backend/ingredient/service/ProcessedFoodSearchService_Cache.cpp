/*
 * 파일 개요: 가공식품 검색 결과 TTL 캐시 구현 파일이다.
 * 주요 역할: 키워드 기반 캐시 조회/저장, 만료 엔트리 제거, 동시성 보호를 담당한다.
 * 사용 위치: ProcessedFoodSearchService가 동일 키워드 재검색 시 외부 API 호출을 줄이기 위해 사용한다.
 */
#include "ingredient/service/ProcessedFoodSearchService_Cache.h"

namespace ingredient
{

// 캐시 hit면 값 반환, miss/만료면 nullopt.
std::optional<ProcessedFoodSearchResultDTO> ProcessedFoodSearchService_Cache::get(
    const std::string &cacheKey)
{
    // 검색 요청이 병렬로 들어오더라도 조회/만료삭제/반환이 원자적으로 보이도록 잠근다.
    std::lock_guard<std::mutex> lock(mutex_);
    const auto it = cache_.find(cacheKey);
    if (it == cache_.end())
    {
        return std::nullopt;
    }

    // 주기적 청소 스레드 대신, 접근 시점에 만료 엔트리를 제거하는 lazy eviction 전략을 쓴다.
    if (std::chrono::steady_clock::now() >= it->second.expiresAt)
    {
        cache_.erase(it);
        return std::nullopt;
    }

    return it->second.result;
}

// 현재 시각 + TTL로 만료시각을 계산해 캐시를 갱신한다.
void ProcessedFoodSearchService_Cache::put(
    const std::string &cacheKey,
    const ProcessedFoodSearchResultDTO &result)
{
    // 같은 키가 이미 있어도 최신 응답과 새 만료시각으로 덮어써 stale 데이터를 짧게 유지한다.
    std::lock_guard<std::mutex> lock(mutex_);
    cache_[cacheKey] = CacheEntry{
        std::chrono::steady_clock::now() + kSearchCacheTtl,
        result,
    };
}

}  // namespace ingredient

