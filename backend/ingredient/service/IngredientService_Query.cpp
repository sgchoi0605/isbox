/*
 * 파일 개요: 식재료 조회(Query) 유스케이스 구현 파일이다.
 * 주요 역할: 인증 유효성 검사 후 사용자 식재료 목록을 조회하고 응답 DTO 포맷으로 정규화한다.
 * 사용 위치: IngredientService 파사드의 조회 경로에서 호출되며 IngredientController의 목록 API 응답 생성에 기여한다.
 */
#include "ingredient/service/IngredientService_Query.h"

#include "ingredient/service/IngredientService_Validation.h"

namespace ingredient
{

// 조회 흐름: 인증 확인 -> 목록 조회 -> 응답 포맷 정규화.
IngredientListResultDTO IngredientService_Query::getIngredients(std::uint64_t memberId)
{
    IngredientListResultDTO result;
    // 인증 정보가 없으면 조회를 진행하지 않는다.
    if (memberId == 0)
    {
        result.statusCode = 401;
        result.message = "Unauthorized.";
        return result;
    }

    try
    {
        // 사용자 소유 식재료만 조회한다.
        result.ingredients = mapper_.findIngredientsByMemberId(memberId);
        // 저장 포맷을 클라이언트 응답 포맷으로 변환한다.
        for (auto &ingredient : result.ingredients)
        {
            IngredientService_Validation::normalizeIngredientForClient(ingredient);
        }

        result.ok = true;
        result.statusCode = 200;
        result.message = "Ingredients loaded.";
        return result;
    }
    // DB/런타임 예외는 공통 500 응답으로 변환한다.
    catch (const std::exception &)
    {
        result.statusCode = 500;
        result.message = "Server error while loading ingredients.";
        return result;
    }
}

}  // namespace ingredient


