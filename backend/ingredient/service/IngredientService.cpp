/*
 * 파일 개요: 식재료 도메인 통합 퍼사드(IngredientService) 구현 파일이다.
 * 주요 역할: Query/Command 하위 서비스를 공통 Mapper로 조립하고, 컨트롤러 요청을 읽기/쓰기 유스케이스로 위임한다.
 * 사용 위치: IngredientController의 식재료 목록 조회/생성/수정/삭제 엔드포인트에서 공통 서비스 진입점으로 사용된다.
 */
#include "ingredient/service/IngredientService.h"

namespace ingredient
{

// Query/Command가 같은 Mapper를 공유하도록 서비스를 연결한다.
IngredientService::IngredientService() : query_(mapper_), command_(mapper_) {}

// 퍼사드 조회 API는 Query 서비스로 위임한다.
IngredientListResultDTO IngredientService::getIngredients(std::uint64_t memberId)
{
    return query_.getIngredients(memberId);
}

// 퍼사드 생성 API는 Command 서비스로 위임한다.
IngredientSingleResultDTO IngredientService::createIngredient(
    std::uint64_t memberId,
    const CreateIngredientRequestDTO &request)
{
    return command_.createIngredient(memberId, request);
}

// 퍼사드 수정 API는 Command 서비스로 위임한다.
IngredientSingleResultDTO IngredientService::updateIngredient(
    std::uint64_t memberId,
    std::uint64_t ingredientId,
    const UpdateIngredientRequestDTO &request)
{
    return command_.updateIngredient(memberId, ingredientId, request);
}

// 퍼사드 삭제 API는 Command 서비스로 위임한다.
IngredientDeleteResultDTO IngredientService::deleteIngredient(
    std::uint64_t memberId,
    std::uint64_t ingredientId)
{
    return command_.deleteIngredient(memberId, ingredientId);
}

}  // namespace ingredient


