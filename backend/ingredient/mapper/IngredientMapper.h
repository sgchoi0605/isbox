/*
 * 파일 개요: ingredients 테이블 접근용 DB 매퍼 인터페이스 선언 파일.
 * 주요 역할: 조회/삽입/수정/삭제 쿼리 함수 시그니처와 row-DTO 변환 진입점을 정의한다.
 * 사용 위치: 서비스 계층이 SQL 세부사항을 직접 다루지 않도록 분리 계층을 제공한다.
 */

#pragma once

#include <drogon/orm/Row.h>

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

#include "ingredient/model/IngredientTypes.h"

namespace ingredient
{

// ingredients 테이블을 대상으로 조회/변경을 수행하는 DB 매퍼.
class IngredientMapper
{
  public:
    // 상태가 없는 매퍼이므로 기본 생성자를 사용한다.
    IngredientMapper() = default;

    // 회원 ID 기준으로 식재료 목록을 조회한다.
    std::vector<IngredientDTO> findIngredientsByMemberId(
        std::uint64_t memberId) const;

    // 회원 소유 여부를 포함해 식재료 단건을 조회한다.
    std::optional<IngredientDTO> findByIdForMember(std::uint64_t ingredientId,
                                                   std::uint64_t memberId) const;

    // 식재료를 INSERT하고 생성된 최종 row를 다시 조회해 반환한다.
    std::optional<IngredientDTO> insertIngredient(
        std::uint64_t memberId,
        const CreateIngredientRequestDTO &request) const;

    // 수정 가능한 필드만 UPDATE한다.
    bool updateEditableFields(std::uint64_t ingredientId,
                              std::uint64_t memberId,
                              const UpdateIngredientRequestDTO &request) const;

    // 식재료를 삭제한다.
    bool markDeleted(std::uint64_t ingredientId, std::uint64_t memberId) const;

  private:
    // DB row를 IngredientDTO로 변환한다.
    static IngredientDTO rowToIngredientDTO(const drogon::orm::Row &row);

    // 로컬 개발 환경에서 필요한 테이블이 없으면 생성한다.
    void ensureTablesForLocalDev() const;
};

}  // namespace ingredient



