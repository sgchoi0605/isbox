#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace ingredient
{

// 식재료 생성 요청 DTO.
class CreateIngredientRequestDTO
{
  public:
    // 기본 식재료 정보.
    std::string name;
    std::string category;
    std::string quantity;
    std::string unit;
    std::string storage;
    std::string expiryDate;

    // 공공 식품코드(가공식품 검색 연동 시 사용).
    std::optional<std::string> publicFoodCode;

    // 영양정보 필드(선택값).
    std::optional<std::string> nutritionBasisAmount;
    std::optional<double> energyKcal;
    std::optional<double> proteinG;
    std::optional<double> fatG;
    std::optional<double> carbohydrateG;
    std::optional<double> sugarG;
    std::optional<double> sodiumMg;

    // 데이터 출처 메타데이터(선택값).
    std::optional<std::string> sourceName;
    std::optional<std::string> manufacturerName;
    std::optional<std::string> importYn;
    std::optional<std::string> originCountryName;
    std::optional<std::string> dataBaseDate;
};

// 식재료 수정 요청 DTO.
class UpdateIngredientRequestDTO
{
  public:
    // 수정 가능한 기본 식재료 정보.
    std::string name;
    std::string category;
    std::string quantity;
    std::string unit;
    std::string storage;
    std::string expiryDate;
};

// 식재료 단건/목록 응답용 DTO.
class IngredientDTO
{
  public:
    // 식재료 PK 및 소유자 member_id.
    std::uint64_t ingredientId{0};
    std::uint64_t memberId{0};

    // 공공 식품코드(선택).
    std::optional<std::string> publicFoodCode;

    // 기본 식재료 정보.
    std::string name;
    std::string category;
    std::string quantity;
    std::string unit;
    std::string storage;
    std::string expiryDate;

    // 영양정보 필드(선택값).
    std::optional<std::string> nutritionBasisAmount;
    std::optional<double> energyKcal;
    std::optional<double> proteinG;
    std::optional<double> fatG;
    std::optional<double> carbohydrateG;
    std::optional<double> sugarG;
    std::optional<double> sodiumMg;

    // 데이터 출처 메타데이터(선택값).
    std::optional<std::string> sourceName;
    std::optional<std::string> manufacturerName;
    std::optional<std::string> importYn;
    std::optional<std::string> originCountryName;
    std::optional<std::string> dataBaseDate;

    // 생성/수정 시각(문자열 포맷).
    std::string createdAt;
    std::string updatedAt;
};

// 가공식품 검색 결과의 단일 아이템 DTO.
class ProcessedFoodSearchItemDTO
{
  public:
    // 식품 기본 식별 정보.
    std::string foodCode;
    std::string foodName;
    std::optional<std::string> foodGroupName;

    // 영양정보 필드(선택값).
    std::optional<std::string> nutritionBasisAmount;
    std::optional<double> energyKcal;
    std::optional<double> proteinG;
    std::optional<double> fatG;
    std::optional<double> carbohydrateG;
    std::optional<double> sugarG;
    std::optional<double> sodiumMg;

    // 데이터 출처 메타데이터(선택값).
    std::optional<std::string> sourceName;
    std::optional<std::string> manufacturerName;
    std::optional<std::string> importYn;
    std::optional<std::string> originCountryName;
    std::optional<std::string> dataBaseDate;
};

// 식재료 목록 조회 서비스 결과 래퍼 DTO.
class IngredientListResultDTO
{
  public:
    bool ok{false};
    int statusCode{400};
    std::string message;
    std::vector<IngredientDTO> ingredients;
};

// 식재료 단건 조회/생성/수정 결과 래퍼 DTO.
class IngredientSingleResultDTO
{
  public:
    bool ok{false};
    int statusCode{400};
    std::string message;
    std::optional<IngredientDTO> ingredient;
};

// 식재료 삭제 결과 래퍼 DTO.
class IngredientDeleteResultDTO
{
  public:
    bool ok{false};
    int statusCode{400};
    std::string message;
};

// 가공식품 검색 결과 래퍼 DTO.
class ProcessedFoodSearchResultDTO
{
  public:
    bool ok{false};
    int statusCode{400};
    std::string message;
    std::vector<ProcessedFoodSearchItemDTO> foods;
};

}  // namespace ingredient
