/*
 * 파일 개요: 식재료 쓰기(Command) 유스케이스 서비스 인터페이스 선언 파일이다.
 * 주요 역할: 생성/수정/삭제 API 계약과 Mapper 의존성 보유 구조를 정의한다.
 * 사용 위치: IngredientService가 내부적으로 포함해 식재료 변경 요청을 처리할 때 사용된다.
 */
#pragma once

#include <cstdint>

#include "ingredient/mapper/IngredientMapper.h"
#include "ingredient/model/IngredientTypes.h"

namespace ingredient
{

// 재료 생성/수정/삭제(write) 유스케이스를 담당한다.
class IngredientService_Command
{
  public:
    explicit IngredientService_Command(IngredientMapper &mapper) : mapper_(mapper) {}

    // 식재료 생성.
    IngredientSingleResultDTO createIngredient(
        std::uint64_t memberId,
        const CreateIngredientRequestDTO &request);
    // 식재료 수정.
    IngredientSingleResultDTO updateIngredient(
        std::uint64_t memberId,
        std::uint64_t ingredientId,
        const UpdateIngredientRequestDTO &request);
    // 식재료 삭제(소프트 삭제).
    IngredientDeleteResultDTO deleteIngredient(std::uint64_t memberId,
                                               std::uint64_t ingredientId);

  private:
    // DB 접근은 상위 서비스에서 주입받은 mapper 하나를 공유해 일관된 저장 규칙을 사용한다.
    IngredientMapper &mapper_;
};

}  // namespace ingredient

