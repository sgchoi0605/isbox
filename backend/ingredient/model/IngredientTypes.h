/*
 * 파일 개요: ingredient 도메인에서 사용하는 요청/응답 DTO 타입 정의 파일.
 * 주요 역할: 컨트롤러-서비스-매퍼 계층 사이에서 공유되는 데이터 계약과 필드 구조를 명세한다.
 * 사용 위치: ingredient 모듈 전반에서 컴파일 타임 타입 안정성을 유지하는 기준 모델로 사용된다.
 */

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
    // 식재료 이름.
    std::string name;
    // 식재료 카테고리.
    std::string category;
    // 수량(문자열 원본 유지).
    std::string quantity;
    // 단위(예: g, ml, 개).
    std::string unit;
    // 보관 위치(클라이언트 값).
    std::string storage;
    // 유통기한(YYYY-MM-DD).
    std::string expiryDate;

    // 공공 식품 코드.
    std::optional<std::string> publicFoodCode;

    // 영양 기준량(예: 100g, 1회 제공량).
    std::optional<std::string> nutritionBasisAmount;
    // 열량(kcal).
    std::optional<double> energyKcal;
    // 단백질(g).
    std::optional<double> proteinG;
    // 지방(g).
    std::optional<double> fatG;
    // 탄수화물(g).
    std::optional<double> carbohydrateG;
    // 당류(g).
    std::optional<double> sugarG;
    // 나트륨(mg).
    std::optional<double> sodiumMg;

    // 데이터 출처명.
    std::optional<std::string> sourceName;
    // 제조사명.
    std::optional<std::string> manufacturerName;
    // 수입 여부(Y/N).
    std::optional<std::string> importYn;
    // 원산지 국가명.
    std::optional<std::string> originCountryName;
    // 데이터 기준일.
    std::optional<std::string> dataBaseDate;
};

// 식재료 수정 요청 DTO.
class UpdateIngredientRequestDTO
{
  public:
    // 식재료 이름.
    std::string name;
    // 식재료 카테고리.
    std::string category;
    // 수량(문자열 원본 유지).
    std::string quantity;
    // 단위(예: g, ml, 개).
    std::string unit;
    // 보관 위치(클라이언트 값).
    std::string storage;
    // 유통기한(YYYY-MM-DD).
    std::string expiryDate;
};

// 식재료 조회/응답 DTO.
class IngredientDTO
{
  public:
    // 식재료 ID(PK).
    std::uint64_t ingredientId{0};
    // 소유 회원 ID(FK).
    std::uint64_t memberId{0};

    // 공공 식품 코드.
    std::optional<std::string> publicFoodCode;

    // 식재료 이름.
    std::string name;
    // 식재료 카테고리.
    std::string category;
    // 수량(문자열 원본 유지).
    std::string quantity;
    // 단위(예: g, ml, 개).
    std::string unit;
    // 보관 위치(클라이언트 값).
    std::string storage;
    // 유통기한(YYYY-MM-DD).
    std::string expiryDate;

    // 영양 기준량(예: 100g, 1회 제공량).
    std::optional<std::string> nutritionBasisAmount;
    // 열량(kcal).
    std::optional<double> energyKcal;
    // 단백질(g).
    std::optional<double> proteinG;
    // 지방(g).
    std::optional<double> fatG;
    // 탄수화물(g).
    std::optional<double> carbohydrateG;
    // 당류(g).
    std::optional<double> sugarG;
    // 나트륨(mg).
    std::optional<double> sodiumMg;

    // 데이터 출처명.
    std::optional<std::string> sourceName;
    // 제조사명.
    std::optional<std::string> manufacturerName;
    // 수입 여부(Y/N).
    std::optional<std::string> importYn;
    // 원산지 국가명.
    std::optional<std::string> originCountryName;
    // 데이터 기준일.
    std::optional<std::string> dataBaseDate;

    // 생성 시각(문자열).
    std::string createdAt;
    // 수정 시각(문자열).
    std::string updatedAt;
};

// 가공식품 검색 단건 DTO.
class ProcessedFoodSearchItemDTO
{
  public:
    // 식품 코드.
    std::string foodCode;
    // 식품명.
    std::string foodName;
    // 식품 분류명.
    std::optional<std::string> foodGroupName;
    // 소스 타입 키(processed/material/food).
    std::string sourceType;
    // 소스 타입 라벨.
    std::string sourceTypeLabel;

    // 영양 기준량(예: 100g, 1회 제공량).
    std::optional<std::string> nutritionBasisAmount;
    // 열량(kcal).
    std::optional<double> energyKcal;
    // 단백질(g).
    std::optional<double> proteinG;
    // 지방(g).
    std::optional<double> fatG;
    // 탄수화물(g).
    std::optional<double> carbohydrateG;
    // 당류(g).
    std::optional<double> sugarG;
    // 나트륨(mg).
    std::optional<double> sodiumMg;

    // 데이터 출처명.
    std::optional<std::string> sourceName;
    // 제조사명.
    std::optional<std::string> manufacturerName;
    // 수입 여부(Y/N).
    std::optional<std::string> importYn;
    // 원산지 국가명.
    std::optional<std::string> originCountryName;
    // 데이터 기준일.
    std::optional<std::string> dataBaseDate;
};

// 식재료 목록 응답 래퍼 DTO.
class IngredientListResultDTO
{
  public:
    // 처리 성공 여부.
    bool ok{false};
    // HTTP 상태 코드.
    int statusCode{400};
    // 사용자 메시지.
    std::string message;
    // 식재료 목록.
    std::vector<IngredientDTO> ingredients;
};

// 식재료 단건 응답 래퍼 DTO.
class IngredientSingleResultDTO
{
  public:
    // 처리 성공 여부.
    bool ok{false};
    // HTTP 상태 코드.
    int statusCode{400};
    // 사용자 메시지.
    std::string message;
    // 식재료 단건 데이터.
    std::optional<IngredientDTO> ingredient;
};

// 식재료 삭제 응답 래퍼 DTO.
class IngredientDeleteResultDTO
{
  public:
    // 처리 성공 여부.
    bool ok{false};
    // HTTP 상태 코드.
    int statusCode{400};
    // 사용자 메시지.
    std::string message;
};

// 가공식품 검색 응답 래퍼 DTO.
class ProcessedFoodSearchResultDTO
{
  public:
    // 처리 성공 여부.
    bool ok{false};
    // HTTP 상태 코드.
    int statusCode{400};
    // 사용자 메시지.
    std::string message;
    // 검색 결과 목록.
    std::vector<ProcessedFoodSearchItemDTO> foods;
};

// 카테고리별 검색 결과 그룹 DTO.
class NutritionFoodsSearchGroupsDTO
{
  public:
    // 가공식품 결과 그룹.
    std::vector<ProcessedFoodSearchItemDTO> processed;
    // 원재료 결과 그룹.
    std::vector<ProcessedFoodSearchItemDTO> material;
    // 음식 결과 그룹.
    std::vector<ProcessedFoodSearchItemDTO> food;
};

// 카테고리별 검색 결과 개수 DTO.
class NutritionFoodsSearchCountsDTO
{
  public:
    // 가공식품 개수.
    std::uint64_t processed{0};
    // 원재료 개수.
    std::uint64_t material{0};
    // 음식 개수.
    std::uint64_t food{0};
};

// 카테고리별 다음 페이지 존재 여부 DTO.
class NutritionFoodsSearchHasMoreDTO
{
  public:
    // 가공식품 다음 페이지 존재 여부.
    bool processed{false};
    // 원재료 다음 페이지 존재 여부.
    bool material{false};
    // 음식 다음 페이지 존재 여부.
    bool food{false};
};

// 통합 영양 검색 응답 래퍼 DTO.
class NutritionFoodsSearchResultDTO
{
  public:
    // 처리 성공 여부.
    bool ok{false};
    // HTTP 상태 코드.
    int statusCode{400};
    // 사용자 메시지.
    std::string message;
    // 카테고리별 결과.
    NutritionFoodsSearchGroupsDTO groups;
    // 카테고리별 개수.
    NutritionFoodsSearchCountsDTO counts;
    // 카테고리별 다음 페이지 존재 여부.
    NutritionFoodsSearchHasMoreDTO hasMore;
    // 경고 메시지 목록.
    std::vector<std::string> warnings;
    // 실패한 카테고리 목록.
    std::vector<std::string> failedCategories;
};

}  // namespace ingredient



