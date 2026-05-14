/*
 * 파일 개요: ingredient 관련 HTTP 엔드포인트 계약(선언부)을 정의한다.
 * 핵심 책임: 라우팅할 핸들러 목록, 응답 직렬화 도우미, 요청 파라미터 추출 함수를 명시한다.
 * 설계 의도: 컨트롤러는 HTTP 입출력만 담당하고, 실제 비즈니스 판단은 service 계층에 위임한다.
 */

#pragma once  // 헤더 중복 포함을 방지해 재정의 오류를 막는다.

#include <drogon/HttpTypes.h>  // HttpStatusCode, HTTP 메서드 열거형 등 Drogon HTTP 타입.
#include <drogon/drogon.h>     // HttpRequestPtr/HttpResponsePtr 및 app() 접근에 필요한 메인 헤더.

#include <cstdint>     // std::uint64_t 같은 고정 폭 정수 타입.
#include <functional>  // std::function(콜백 타입 별칭)에 필요.
#include <optional>    // std::optional(선택 값 표현)에 필요.

#include "ingredient/service/IngredientService.h"            // 식재료 CRUD 비즈니스 로직 서비스.
#include "ingredient/service/NutritionFoodsSearchService.h"  // 통합 영양 검색 비즈니스 로직 서비스.
#include "ingredient/service/ProcessedFoodSearchService.h"   // 가공식품 검색 비즈니스 로직 서비스.

namespace ingredient
{

// 식재료 CRUD 및 영양 검색 API를 HTTP 라우트와 연결하는 진입 컨트롤러.
class IngredientController
{
  public:
    // 기본 생성자: 내부 service 멤버를 기본 생성해 즉시 사용 가능한 상태로 만든다.
    IngredientController() = default;

    // 모든 엔드포인트(GET/POST/PUT/DELETE)를 Drogon 앱 라우터에 등록한다.
    void registerHandlers();

  private:
    // 응답 전송 콜백 타입: Drogon이 요구하는 "HttpResponsePtr 전달" 시그니처를 고정한다.
    using Callback = std::function<void(const drogon::HttpResponsePtr &)>;

    // IngredientDTO(도메인 DTO)를 API 계약에 맞는 JSON 객체로 직렬화한다.
    static Json::Value buildIngredientJson(const IngredientDTO &ingredient);

    // ProcessedFoodSearchItemDTO(검색 결과 DTO)를 API 응답 JSON 객체로 직렬화한다.
    static Json::Value buildProcessedFoodJson(
        const ProcessedFoodSearchItemDTO &food);

    // 요청 Origin을 반영해 CORS 허용 헤더를 일관되게 주입한다.
    static void applyCors(const drogon::HttpRequestPtr &request,
                          const drogon::HttpResponsePtr &response);

    // 인증용 X-Member-Id 헤더를 파싱해 uint64 회원 ID로 변환한다(실패 시 nullopt).
    static std::optional<std::uint64_t> extractMemberIdHeader(
        const drogon::HttpRequestPtr &request);

    // 쿼리스트링 ingredientId 값을 파싱해 uint64 식재료 ID로 변환한다(실패 시 nullopt).
    static std::optional<std::uint64_t> extractIngredientIdParameter(
        const drogon::HttpRequestPtr &request);

    // GET /api/ingredients 요청을 처리해 내 식재료 목록 응답을 반환한다.
    void handleListIngredients(const drogon::HttpRequestPtr &request,
                               Callback &&callback);

    // POST /api/ingredients 요청을 처리해 식재료 생성 응답을 반환한다.
    void handleCreateIngredient(const drogon::HttpRequestPtr &request,
                                Callback &&callback);

    // PUT /api/ingredients 요청을 처리해 식재료 수정 응답을 반환한다.
    void handleUpdateIngredient(const drogon::HttpRequestPtr &request,
                                Callback &&callback);

    // DELETE /api/ingredients 요청을 처리해 식재료 삭제 응답을 반환한다.
    void handleDeleteIngredient(const drogon::HttpRequestPtr &request,
                                Callback &&callback);

    // GET /api/nutrition/processed-foods 요청을 처리해 가공식품 검색 응답을 반환한다.
    void handleSearchProcessedFoods(const drogon::HttpRequestPtr &request,
                                    Callback &&callback);

    // GET /api/nutrition/foods 요청을 처리해 통합 영양 검색 응답을 반환한다.
    void handleSearchFoods(const drogon::HttpRequestPtr &request,
                           Callback &&callback);

    // 식재료 CRUD 유스케이스(조회/생성/수정/삭제)를 수행하는 서비스 객체.
    IngredientService ingredientService_;

    // 가공식품 키워드 검색 유스케이스를 수행하는 서비스 객체.
    ProcessedFoodSearchService processedFoodSearchService_;

    // 통합 식품 검색(카테고리 그룹/경고/카운트 포함)을 수행하는 서비스 객체.
    NutritionFoodsSearchService nutritionFoodsSearchService_;
};

}  // namespace ingredient
