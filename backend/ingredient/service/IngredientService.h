/*
 * 파일 개요: 식재료 도메인 통합 서비스(IngredientService) 인터페이스 선언 파일이다.
 * 주요 역할: 조회(Query)와 변경(Command) 기능을 하나의 파사드 API로 노출하고 내부 의존성 조합 지점을 정의한다.
 * 사용 위치: IngredientController가 식재료 관련 HTTP 요청을 처리할 때 직접 참조하는 서비스 타입으로 사용된다.
 */
#pragma once

#include <cstdint>

#include "ingredient/model/IngredientTypes.h"
#include "ingredient/service/IngredientService_Command.h"
#include "ingredient/service/IngredientService_Query.h"

namespace ingredient
{

// 컨트롤러가 단일 진입점으로 조회/명령 기능을 호출할 수 있도록 묶는 퍼사드다.
class IngredientService
{
  public:
    // Query/Command가 같은 Mapper를 사용하도록 초기화한다.
    IngredientService();

    // 사용자 식재료 목록 조회를 Query 계층으로 위임한다.
    IngredientListResultDTO getIngredients(std::uint64_t memberId);
    // 신규 식재료 생성을 Command 계층으로 위임한다.
    IngredientSingleResultDTO createIngredient(
        std::uint64_t memberId,
        const CreateIngredientRequestDTO &request);
    // 식재료 수정을 Command 계층으로 위임한다.
    IngredientSingleResultDTO updateIngredient(
        std::uint64_t memberId,
        std::uint64_t ingredientId,
        const UpdateIngredientRequestDTO &request);
    // 식재료 삭제를 Command 계층으로 위임한다.
    IngredientDeleteResultDTO deleteIngredient(std::uint64_t memberId,
                                               std::uint64_t ingredientId);

  private:
    // Query/Command가 공유하는 DB 접근 객체.
    IngredientMapper mapper_;
    // 읽기 유스케이스 서비스.
    IngredientService_Query query_;
    // 쓰기 유스케이스 서비스.
    IngredientService_Command command_;
};

}  // namespace ingredient


