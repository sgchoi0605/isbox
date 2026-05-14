/*
 * 파일 개요: 검색 결과 조립(Assembler) 컴포넌트 인터페이스 선언 파일이다.
 * 주요 역할: row 병합/필터링, DTO 매핑, 우선순위 정렬 API 계약을 제공한다.
 * 사용 위치: 검색 서비스 계층이 공공 API 응답을 도메인 DTO로 변환할 때 공통 조립기로 사용된다.
 */
#pragma once

#include <string>
#include <unordered_set>
#include <vector>

#include <json/value.h>

#include "ingredient/model/IngredientTypes.h"

namespace ingredient
{

// API 원본 row를 검색 응답 DTO로 조립하는 변환 계층.
class ProcessedFoodSearchService_Assembler
{
  public:
    // foodCode 기준 중복 제거 + 키워드 필터를 적용해 row를 병합한다.
    void appendRows(std::vector<Json::Value> &mergedRows,
                    std::unordered_set<std::string> &seenFoodCodes,
                    const std::vector<Json::Value> &rows,
                    bool filterByKeyword,
                    const std::string &normalizedKeywordToken) const;

    // 제조사명 집합 추출(연관 검색 조건 생성용).
    std::unordered_set<std::string> collectManufacturerNames(
        const std::vector<Json::Value> &rows) const;
    // 식품 분류(level4) 집합 추출(연관 검색 조건 생성용).
    std::unordered_set<std::string> collectFoodLevel4Names(
        const std::vector<Json::Value> &rows) const;

    // row 배열을 DTO 배열로 변환한다.
    std::vector<ProcessedFoodSearchItemDTO> mapRowsToFoods(
        const std::vector<Json::Value> &rows,
        bool dedupeByFoodName) const;

    // 검색 우선순위(exact/prefix/partial)로 정렬한다.
    void sortFoods(std::vector<ProcessedFoodSearchItemDTO> &foods,
                   const std::string &normalizedKeywordToken) const;
};

}  // namespace ingredient


