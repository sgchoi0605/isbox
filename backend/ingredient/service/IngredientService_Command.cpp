/*
 * 파일 개요: 식재료 생성/수정/삭제(Command) 유스케이스 구현 파일이다.
 * 주요 역할: 인증 확인, 입력 정규화/검증, Mapper 저장 호출, 예외를 API 응답 코드로 변환하는 쓰기 흐름을 담당한다.
 * 사용 위치: IngredientService 파사드의 쓰기 경로에서 호출되며 ingredient mapper 계층과 직접 연동된다.
 */
#include "ingredient/service/IngredientService_Command.h"

#include <optional>

#include "ingredient/service/IngredientService_Validation.h"

namespace ingredient
{

// 생성 흐름: 인증 -> 입력 정규화/검증 -> DB 저장 -> 응답 정규화.
IngredientSingleResultDTO IngredientService_Command::createIngredient(
    std::uint64_t memberId,
    const CreateIngredientRequestDTO &request)
{
    IngredientSingleResultDTO result;
    // 인증되지 않은 요청은 즉시 거절한다.
    if (memberId == 0)
    {
        result.statusCode = 401;
        result.message = "Unauthorized.";
        return result;
    }

    // 공백/대소문자 차이로 인한 검증 누락을 막기 위해 먼저 정규화한다.
    const auto name = IngredientService_Validation::trim(request.name);
    const auto category = IngredientService_Validation::trim(request.category);
    const auto quantity = IngredientService_Validation::trim(request.quantity);
    const auto unit = IngredientService_Validation::trim(request.unit);
    const auto storage = IngredientService_Validation::toLower(
        IngredientService_Validation::trim(request.storage));
    const auto expiryDate = IngredientService_Validation::trim(request.expiryDate);

    // 필수값 누락을 먼저 차단한다.
    if (name.empty() || category.empty() || quantity.empty() || unit.empty() ||
        storage.empty() || expiryDate.empty())
    {
        result.statusCode = 400;
        result.message = "Required ingredient fields are missing.";
        return result;
    }

    // 길이 제약은 DB 반영 전에 선검증한다.
    if (name.size() > 150U || category.size() > 50U || quantity.size() > 50U ||
        unit.size() > 20U)
    {
        result.statusCode = 400;
        result.message = "Ingredient field length exceeds limit.";
        return result;
    }

    // storage는 허용된 enum 값만 통과시킨다.
    if (!IngredientService_Validation::isAllowedClientStorage(storage))
    {
        result.statusCode = 400;
        result.message = "Invalid storage value.";
        return result;
    }

    // 날짜 형식은 YYYY-MM-DD만 허용한다.
    if (!IngredientService_Validation::isValidDate(expiryDate))
    {
        result.statusCode = 400;
        result.message = "Invalid expiry date format.";
        return result;
    }

    // 저장용 DTO로 재구성하면서 storage 포맷을 DB enum으로 맞춘다.
    CreateIngredientRequestDTO normalized = request;
    normalized.name = name;
    normalized.category = category;
    normalized.quantity = quantity;
    normalized.unit = unit;
    normalized.storage = IngredientService_Validation::toDbStorage(storage);
    normalized.expiryDate = expiryDate;

    // 선택 필드의 빈 문자열은 null로 정리해 불필요한 값 저장을 막는다.
    auto normalizeOptionalTrim = [](std::optional<std::string> &value) {
        if (!value.has_value())
        {
            return;
        }
        if (value->empty())
        {
            value = std::nullopt;
        }
    };

    normalizeOptionalTrim(normalized.publicFoodCode);
    normalizeOptionalTrim(normalized.nutritionBasisAmount);
    normalizeOptionalTrim(normalized.sourceName);
    normalizeOptionalTrim(normalized.manufacturerName);
    normalizeOptionalTrim(normalized.originCountryName);

    // import/date는 API-DB 표기 차이를 흡수해 정규화한다.
    normalized.importYn =
        IngredientService_Validation::normalizeImportYn(normalized.importYn);
    normalized.dataBaseDate =
        IngredientService_Validation::normalizeDataBaseDate(normalized.dataBaseDate);

    try
    {
        // 저장 성공 후 반환값을 응답 포맷으로 다시 맞춘다.
        auto created = mapper_.insertIngredient(memberId, normalized);
        if (!created.has_value())
        {
            result.statusCode = 500;
            result.message = "Failed to create ingredient.";
            return result;
        }

        IngredientService_Validation::normalizeIngredientForClient(*created);

        result.ok = true;
        result.statusCode = 201;
        result.message = "Ingredient created.";
        result.ingredient = created;
        return result;
    }
    // 예외는 공통 500 오류로 변환한다.
    catch (const std::exception &)
    {
        result.statusCode = 500;
        result.message = "Server error while creating ingredient.";
        return result;
    }
}

// 수정 흐름: 인증/대상 확인 -> 입력 검증 -> 변경 반영 -> 최신값 재조회.
IngredientSingleResultDTO IngredientService_Command::updateIngredient(
    std::uint64_t memberId,
    std::uint64_t ingredientId,
    const UpdateIngredientRequestDTO &request)
{
    IngredientSingleResultDTO result;
    // 인증되지 않은 요청은 즉시 거절한다.
    if (memberId == 0)
    {
        result.statusCode = 401;
        result.message = "Unauthorized.";
        return result;
    }
    // 수정 대상 식별자가 없으면 처리하지 않는다.
    if (ingredientId == 0)
    {
        result.statusCode = 400;
        result.message = "Ingredient id is required.";
        return result;
    }

    // 생성과 같은 규칙으로 입력을 정규화/검증한다.
    const auto name = IngredientService_Validation::trim(request.name);
    const auto category = IngredientService_Validation::trim(request.category);
    const auto quantity = IngredientService_Validation::trim(request.quantity);
    const auto unit = IngredientService_Validation::trim(request.unit);
    const auto storage = IngredientService_Validation::toLower(
        IngredientService_Validation::trim(request.storage));
    const auto expiryDate = IngredientService_Validation::trim(request.expiryDate);

    if (name.empty() || category.empty() || quantity.empty() || unit.empty() ||
        storage.empty() || expiryDate.empty())
    {
        result.statusCode = 400;
        result.message = "Required ingredient fields are missing.";
        return result;
    }

    if (name.size() > 150U || category.size() > 50U || quantity.size() > 50U ||
        unit.size() > 20U)
    {
        result.statusCode = 400;
        result.message = "Ingredient field length exceeds limit.";
        return result;
    }

    if (!IngredientService_Validation::isAllowedClientStorage(storage))
    {
        result.statusCode = 400;
        result.message = "Invalid storage value.";
        return result;
    }

    if (!IngredientService_Validation::isValidDate(expiryDate))
    {
        result.statusCode = 400;
        result.message = "Invalid expiry date format.";
        return result;
    }

    try
    {
        // 사용자 소유 데이터인지 먼저 확인한다.
        const auto existing = mapper_.findByIdForMember(ingredientId, memberId);
        if (!existing.has_value())
        {
            result.statusCode = 404;
            result.message = "Ingredient not found.";
            return result;
        }

        // 수정 가능한 필드만 별도 DTO로 구성해 업데이트 범위를 명확히 한다.
        UpdateIngredientRequestDTO normalized;
        normalized.name = name;
        normalized.category = category;
        normalized.quantity = quantity;
        normalized.unit = unit;
        normalized.storage = IngredientService_Validation::toDbStorage(storage);
        normalized.expiryDate = expiryDate;

        if (!mapper_.updateEditableFields(ingredientId, memberId, normalized))
        {
            result.statusCode = 500;
            result.message = "Failed to update ingredient.";
            return result;
        }

        // 반영 후 재조회 결과를 응답으로 사용한다.
        auto updated = mapper_.findByIdForMember(ingredientId, memberId);
        if (!updated.has_value())
        {
            result.statusCode = 500;
            result.message = "Failed to load updated ingredient.";
            return result;
        }

        IngredientService_Validation::normalizeIngredientForClient(*updated);

        result.ok = true;
        result.statusCode = 200;
        result.message = "Ingredient updated.";
        result.ingredient = updated;
        return result;
    }
    // 예외는 공통 500 오류로 변환한다.
    catch (const std::exception &)
    {
        result.statusCode = 500;
        result.message = "Server error while updating ingredient.";
        return result;
    }
}

// 삭제 흐름: 인증/대상 확인 -> 소프트 삭제 플래그 반영.
IngredientDeleteResultDTO IngredientService_Command::deleteIngredient(
    std::uint64_t memberId,
    std::uint64_t ingredientId)
{
    IngredientDeleteResultDTO result;
    // 인증되지 않은 요청은 즉시 거절한다.
    if (memberId == 0)
    {
        result.statusCode = 401;
        result.message = "Unauthorized.";
        return result;
    }
    // 삭제 대상 식별자가 없으면 처리하지 않는다.
    if (ingredientId == 0)
    {
        result.statusCode = 400;
        result.message = "Ingredient id is required.";
        return result;
    }

    try
    {
        // 사용자 소유 데이터인지 먼저 확인한다.
        const auto existing = mapper_.findByIdForMember(ingredientId, memberId);
        if (!existing.has_value())
        {
            result.statusCode = 404;
            result.message = "Ingredient not found.";
            return result;
        }

        // 실제 물리 삭제 대신 삭제 상태만 변경한다.
        if (!mapper_.markDeleted(ingredientId, memberId))
        {
            result.statusCode = 500;
            result.message = "Failed to delete ingredient.";
            return result;
        }

        result.ok = true;
        result.statusCode = 200;
        result.message = "Ingredient deleted.";
        return result;
    }
    // 예외는 공통 500 오류로 변환한다.
    catch (const std::exception &)
    {
        result.statusCode = 500;
        result.message = "Server error while deleting ingredient.";
        return result;
    }
}

}  // namespace ingredient


