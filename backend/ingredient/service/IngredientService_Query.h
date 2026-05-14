/*
 * 파일 개요: 식재료 조회(Query) 유스케이스 서비스 인터페이스 선언 파일이다.
 * 주요 역할: memberId 기준 식재료 목록 조회 계약과 Mapper 의존성 연결 지점을 정의한다.
 * 사용 위치: IngredientService가 내부 조회 전용 서브 서비스로 보유해 read 경로를 분리하는 데 사용된다.
 */
#pragma once

#include <cstdint>

#include "ingredient/mapper/IngredientMapper.h"
#include "ingredient/model/IngredientTypes.h"

namespace ingredient
{

// 재료 조회(read) 유스케이스를 담당한다.
class IngredientService_Query
{
  public:
    explicit IngredientService_Query(IngredientMapper &mapper) : mapper_(mapper) {}

    // 사용자 식재료 목록을 조회해 응답 DTO로 반환한다.
    IngredientListResultDTO getIngredients(std::uint64_t memberId);

  private:
    // 조회 전용 서비스지만 동일한 mapper 계층을 사용해 Query/Command 간 데이터 해석 규칙을 맞춘다.
    IngredientMapper &mapper_;
};

}  // namespace ingredient

