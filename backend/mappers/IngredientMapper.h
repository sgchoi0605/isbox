#pragma once

#include <drogon/orm/Row.h>

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

#include "../models/IngredientTypes.h"

namespace ingredient
{

// ingredients 테이블을 대상으로 동작하는 데이터 접근 계층이다.
// 서비스 레이어는 SQL을 직접 다루지 않고, 이 Mapper를 통해서만 조회/변경한다.
class IngredientMapper
{
  public:
    // 별도 상태 없이 동작하므로 기본 생성자를 사용한다.
    IngredientMapper() = default;

    // 회원 1명의 활성(미삭제) 재료 목록을 조회한다.
    // 보관 방법, 유통기한 기준으로 정렬해 화면에서 바로 사용 가능하게 반환한다.
    std::vector<IngredientDTO> findIngredientsByMemberId(
        std::uint64_t memberId) const;

    // 특정 회원의 특정 재료 1건을 조회한다.
    // 회원 소유권 검증(member_id 일치)과 미삭제 조건을 함께 적용한다.
    std::optional<IngredientDTO> findByIdForMember(std::uint64_t ingredientId,
                                                   std::uint64_t memberId) const;

    // 신규 재료를 등록하고, 등록된 row를 DTO 형태로 다시 조회해 반환한다.
    std::optional<IngredientDTO> insertIngredient(
        std::uint64_t memberId,
        const CreateIngredientRequestDTO &request) const;

    // 사용자가 수정 가능한 필드만 업데이트한다.
    // 성공적으로 갱신된 row가 있으면 true를 반환한다.
    bool updateEditableFields(std::uint64_t ingredientId,
                              std::uint64_t memberId,
                              const UpdateIngredientRequestDTO &request) const;

    // 물리 삭제 대신 deleted_at을 채우는 소프트 삭제를 수행한다.
    // 실제로 상태가 바뀐 경우 true를 반환한다.
    bool markDeleted(std::uint64_t ingredientId, std::uint64_t memberId) const;

  private:
    // DB row를 API 응답용 IngredientDTO로 변환한다.
    static IngredientDTO rowToIngredientDTO(const drogon::orm::Row &row);

    // 로컬 개발 환경에서 필요한 테이블이 없으면 최소 스키마를 생성한다.
    void ensureTablesForLocalDev() const;
};

}  // namespace ingredient
