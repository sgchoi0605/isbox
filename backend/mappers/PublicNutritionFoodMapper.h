#pragma once

#include <drogon/orm/Row.h>

#include <cstddef>
#include <string>
#include <vector>

#include "../models/IngredientTypes.h"

namespace ingredient
{

// 공공 영양 API 원본을 로컬 캐시 테이블에 저장하고 검색한다.
class PublicNutritionFoodMapper
{
  public:
    PublicNutritionFoodMapper() = default;

    // 캐시된 전체 row 개수를 반환한다.
    std::size_t countFoods() const;

    // 공공 API 결과를 food_code 기준으로 upsert한다.
    void upsertFoods(const std::vector<ProcessedFoodSearchItemDTO> &foods) const;

    // normalized_food_name LIKE 검색 후 food_name 기준 dedupe 결과를 반환한다.
    std::vector<ProcessedFoodSearchItemDTO> searchByKeyword(
        const std::string &keyword) const;

    // 검색용 정규화(공백 제거 + 소문자) 문자열을 만든다.
    static std::string normalizeForSearch(std::string value);

  private:
    // DB row를 검색 응답 DTO로 변환한다.
    static ProcessedFoodSearchItemDTO rowToDTO(const drogon::orm::Row &row);

    // representative row 선택 시 영양 필드 채움 점수를 계산한다.
    static int nutritionFilledFieldCount(const ProcessedFoodSearchItemDTO &food);

    // representative row 비교: lhs가 rhs보다 우선이면 true.
    static bool isRepresentativeBetter(const ProcessedFoodSearchItemDTO &lhs,
                                       const ProcessedFoodSearchItemDTO &rhs);

    // 검색 결과 정렬 우선순위를 계산한다.
    static int searchPriority(const ProcessedFoodSearchItemDTO &food,
                              const std::string &normalizedKeyword);

    // 로컬 개발 시 캐시 테이블/인덱스를 보장한다.
    void ensureTableForLocalDev() const;
};

}  // namespace ingredient

