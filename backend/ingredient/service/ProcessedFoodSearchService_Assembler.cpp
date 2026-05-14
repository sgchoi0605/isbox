/*
 * 파일 개요: 공공 API 원본 row를 검색 응답 DTO로 조립하는 변환 계층 구현 파일이다.
 * 주요 역할: 중복 제거 병합, 연관 검색 조건 추출, alias 기반 필드 매핑, 검색 우선순위 정렬을 담당한다.
 * 사용 위치: ProcessedFoodSearchService 및 NutritionFoodsSearchService에서 결과 조립 단계에 공통으로 사용된다.
 */
#include "ingredient/service/ProcessedFoodSearchService_Assembler.h"

#include <algorithm>
#include <utility>

#include "ingredient/service/ProcessedFoodSearchService_Text.h"

namespace ingredient
{

// 병합 단계: 필수 필드 확인 -> (옵션)키워드 포함 여부 -> foodCode 중복 제거.
void ProcessedFoodSearchService_Assembler::appendRows(
    std::vector<Json::Value> &mergedRows,
    std::unordered_set<std::string> &seenFoodCodes,
    const std::vector<Json::Value> &rows,
    bool filterByKeyword,
    const std::string &normalizedKeywordToken) const
{
    for (const auto &row : rows)
    {
        const auto foodCode = ProcessedFoodSearchService_Text::firstStringFromKeys(
            row, {"foodCd", "FOOD_CD"});
        const auto foodName = ProcessedFoodSearchService_Text::firstStringFromKeys(
            row, {"foodNm", "FOOD_NM"});
        // foodCode/foodName이 없으면 검색 후보로 사용할 수 없다.
        if (!foodCode.has_value() || !foodName.has_value())
        {
            continue;
        }

        // 연관 검색 row는 원 키워드 토큰이 포함된 항목만 유지한다.
        if (filterByKeyword &&
            !ProcessedFoodSearchService_Text::containsNormalizedToken(
                *foodName, normalizedKeywordToken))
        {
            continue;
        }

        // 이미 본 foodCode면 중복으로 제외한다.
        if (!seenFoodCodes.insert(*foodCode).second)
        {
            continue;
        }

        mergedRows.push_back(row);
    }
}

// 1차 결과에서 제조사명을 추출해 확장 검색 조건으로 사용한다.
std::unordered_set<std::string> ProcessedFoodSearchService_Assembler::collectManufacturerNames(
    const std::vector<Json::Value> &rows) const
{
    std::unordered_set<std::string> manufacturerNames;
    for (const auto &row : rows)
    {
        if (const auto manufacturerName =
                ProcessedFoodSearchService_Text::firstStringFromKeys(
                    row, {"mfrNm", "MFR_NM", "restNm", "REST_NM"});
            manufacturerName.has_value())
        {
            const auto normalized = ProcessedFoodSearchService_Text::trim(*manufacturerName);
            if (!normalized.empty())
            {
                manufacturerNames.insert(normalized);
            }
        }
    }
    return manufacturerNames;
}

// 1차 결과에서 분류(level4)명을 추출해 확장 검색 조건으로 사용한다.
std::unordered_set<std::string> ProcessedFoodSearchService_Assembler::collectFoodLevel4Names(
    const std::vector<Json::Value> &rows) const
{
    std::unordered_set<std::string> foodLevel4Names;
    for (const auto &row : rows)
    {
        if (const auto foodLevel4Name =
                ProcessedFoodSearchService_Text::firstStringFromKeys(
                    row, {"foodLv4Nm", "FOOD_LV4_NM"});
            foodLevel4Name.has_value())
        {
            const auto normalized = ProcessedFoodSearchService_Text::trim(*foodLevel4Name);
            if (!normalized.empty())
            {
                foodLevel4Names.insert(normalized);
            }
        }
    }
    return foodLevel4Names;
}

// key alias를 순회하며 첫 유효 값을 선택해 DTO를 채운다.
std::vector<ProcessedFoodSearchItemDTO> ProcessedFoodSearchService_Assembler::mapRowsToFoods(
    const std::vector<Json::Value> &rows,
    bool dedupeByFoodName) const
{
    std::vector<ProcessedFoodSearchItemDTO> foods;
    foods.reserve(rows.size());

    std::unordered_set<std::string> seenFoodNames;
    if (dedupeByFoodName)
    {
        seenFoodNames.reserve(rows.size());
    }

    for (const auto &row : rows)
    {
        auto foodCode = ProcessedFoodSearchService_Text::firstStringFromKeys(
            row, {"foodCd", "FOOD_CD"});
        auto foodName = ProcessedFoodSearchService_Text::firstStringFromKeys(
            row, {"foodNm", "FOOD_NM"});
        if (!foodCode.has_value() || !foodName.has_value())
        {
            continue;
        }

        // 이름 중복 제거 옵션이 켜진 경우 동일 foodName은 하나만 남긴다.
        if (dedupeByFoodName && !seenFoodNames.insert(*foodName).second)
        {
            continue;
        }

        ProcessedFoodSearchItemDTO dto;
        dto.foodCode = *foodCode;
        dto.foodName = *foodName;
        dto.foodGroupName = ProcessedFoodSearchService_Text::firstStringFromKeys(
            row,
            {"foodLv6Nm", "foodLv5Nm", "foodLv4Nm", "foodLv3Nm",
             "foodLv7Nm", "FOOD_LV6_NM", "FOOD_LV5_NM", "FOOD_LV4_NM",
             "FOOD_LV3_NM", "FOOD_LV7_NM"});
        dto.nutritionBasisAmount = ProcessedFoodSearchService_Text::firstStringFromKeys(
            row, {"nutConSrtrQua", "NUT_CON_SRTR_QUA"});
        dto.energyKcal = ProcessedFoodSearchService_Text::firstDoubleFromKeys(
            row, {"enerc", "ENERC"});
        dto.proteinG = ProcessedFoodSearchService_Text::firstDoubleFromKeys(
            row, {"prot", "PROT"});
        dto.fatG = ProcessedFoodSearchService_Text::firstDoubleFromKeys(
            row, {"fatce", "FATCE"});
        dto.carbohydrateG = ProcessedFoodSearchService_Text::firstDoubleFromKeys(
            row, {"chocdf", "CHOCDF"});
        dto.sugarG = ProcessedFoodSearchService_Text::firstDoubleFromKeys(
            row, {"sugar", "SUGAR"});
        dto.sodiumMg = ProcessedFoodSearchService_Text::firstDoubleFromKeys(
            row, {"nat", "NAT"});
        dto.sourceName = ProcessedFoodSearchService_Text::firstStringFromKeys(
            row, {"srcNm", "SRC_NM", "instt_nm", "INSTT_NM"});
        dto.manufacturerName = ProcessedFoodSearchService_Text::firstStringFromKeys(
            row, {"mfrNm", "MFR_NM", "restNm", "REST_NM"});
        dto.importYn = ProcessedFoodSearchService_Text::normalizeImportYnForApi(
            ProcessedFoodSearchService_Text::firstStringFromKeys(
                row, {"imptYn", "IMPT_YN"}));
        dto.originCountryName = ProcessedFoodSearchService_Text::firstStringFromKeys(
            row, {"cooNm", "COO_NM", "foodCooRgnNm", "FOOD_COO_RGN_NM"});
        dto.dataBaseDate = ProcessedFoodSearchService_Text::normalizeDataBaseDate(
            ProcessedFoodSearchService_Text::firstStringFromKeys(
                row, {"crtrYmd", "CRTR_YMD", "crtYmd", "CRT_YMD"}));
        foods.push_back(std::move(dto));
    }

    return foods;
}

// 우선순위가 같으면 이름 사전순으로 정렬해 결과 순서를 안정화한다.
void ProcessedFoodSearchService_Assembler::sortFoods(
    std::vector<ProcessedFoodSearchItemDTO> &foods,
    const std::string &normalizedKeywordToken) const
{
    std::sort(foods.begin(),
              foods.end(),
              [&normalizedKeywordToken](const ProcessedFoodSearchItemDTO &lhs,
                                        const ProcessedFoodSearchItemDTO &rhs) {
                  const auto lhsPriority =
                      ProcessedFoodSearchService_Text::searchPriority(
                          lhs, normalizedKeywordToken);
                  const auto rhsPriority =
                      ProcessedFoodSearchService_Text::searchPriority(
                          rhs, normalizedKeywordToken);
                  if (lhsPriority != rhsPriority)
                  {
                      return lhsPriority < rhsPriority;
                  }
                  return lhs.foodName < rhs.foodName;
              });
}

}  // namespace ingredient


