#pragma once

#include <drogon/HttpTypes.h>
#include <drogon/drogon.h>

#include <cstdint>
#include <functional>
#include <optional>

#include "../services/IngredientService.h"
#include "../services/ProcessedFoodSearchService.h"

namespace ingredient
{

// 식재료 및 가공식품 검색 API 요청을 처리하는 컨트롤러
class IngredientController
{
  public:
    IngredientController() = default;

    // 식재료 CRUD 및 가공식품 검색 핸들러를 등록한다.
    void registerHandlers();

  private:
    // Drogon 응답 콜백 타입 별칭
    using Callback = std::function<void(const drogon::HttpResponsePtr &)>;

    // 식재료 DTO를 API 응답용 JSON으로 변환한다.
    static Json::Value buildIngredientJson(const IngredientDTO &ingredient);

    // 가공식품 검색 결과 DTO를 API 응답용 JSON으로 변환한다.
    static Json::Value buildProcessedFoodJson(
        const ProcessedFoodSearchItemDTO &food);

    // 공통 CORS 헤더를 응답에 추가한다.
    static void applyCors(const drogon::HttpRequestPtr &request,
                          const drogon::HttpResponsePtr &response);

    // 인증 헤더(X-Member-Id)에서 멤버 ID를 추출한다.
    static std::optional<std::uint64_t> extractMemberIdHeader(
        const drogon::HttpRequestPtr &request);

    // 쿼리 파라미터 ingredientId를 추출한다.
    static std::optional<std::uint64_t> extractIngredientIdParameter(
        const drogon::HttpRequestPtr &request);

    // GET /api/ingredients 요청을 처리한다.
    void handleListIngredients(const drogon::HttpRequestPtr &request,
                               Callback &&callback);

    // POST /api/ingredients 요청을 처리한다.
    void handleCreateIngredient(const drogon::HttpRequestPtr &request,
                                Callback &&callback);

    // PUT /api/ingredients 요청을 처리한다.
    void handleUpdateIngredient(const drogon::HttpRequestPtr &request,
                                Callback &&callback);

    // DELETE /api/ingredients 요청을 처리한다.
    void handleDeleteIngredient(const drogon::HttpRequestPtr &request,
                                Callback &&callback);

    // GET /api/nutrition/processed-foods 요청을 처리한다.
    void handleSearchProcessedFoods(const drogon::HttpRequestPtr &request,
                                    Callback &&callback);

    // 식재료 CRUD 도메인 비즈니스 로직 서비스 객체
    IngredientService ingredientService_;

    // 가공식품 검색 전용 서비스 객체
    ProcessedFoodSearchService processedFoodSearchService_;
};

}  // namespace ingredient

