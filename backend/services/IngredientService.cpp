#include "IngredientService.h"

#include <algorithm>
#include <cctype>
#include <exception>
#include <regex>

namespace
{

// JSON 값을 비어 있지 않은 날짜 문자열로 정규화한다.
std::optional<std::string> normalizeDataBaseDate(
    const std::optional<std::string> &value)
{
    // 공공 API는 "20240512"처럼 구분자 없는 날짜를 주는 경우가 있어 변환한다.
    // 값이 없으면 nullopt, 8자리 숫자면 YYYY-MM-DD 형식으로 바꾼다.
    if (!value.has_value())
    {
        return std::nullopt;
    }

    std::string text = *value;
    text.erase(
        std::remove_if(text.begin(), text.end(), [](unsigned char ch) {
            return std::isspace(ch) != 0;
        }),
        text.end());

    if (text.empty())
    {
        return std::nullopt;
    }

    static const std::regex yyyymmdd("^[0-9]{8}$");
    if (std::regex_match(text, yyyymmdd))
    {
        return text.substr(0, 4) + "-" + text.substr(4, 2) + "-" +
               text.substr(6, 2);
    }

    return text;
}
}  // 익명 네임스페이스

namespace ingredient
{

IngredientListResultDTO IngredientService::getIngredients(std::uint64_t memberId)
{
    IngredientListResultDTO result;

    // memberId가 없으면 인증 실패로 처리한다.
    if (memberId == 0)
    {
        result.statusCode = 401;
        result.message = "Unauthorized.";
        return result;
    }

    try
    {
        // DB에서 회원의 식재료 목록을 조회한다.
        // 조회 결과는 저장 방식과 보조 필드 형식을 클라이언트 규격으로 맞춘다.
        result.ingredients = mapper_.findIngredientsByMemberId(memberId);

        // 응답 전에 각 항목 표현을 API 형식으로 정규화한다.
        for (auto &ingredient : result.ingredients)
        {
            normalizeIngredientForClient(ingredient);
        }

        result.ok = true;
        result.statusCode = 200;
        result.message = "Ingredients loaded.";
        return result;
    }
    catch (const std::exception &)
    {
        result.statusCode = 500;
        result.message = "Server error while loading ingredients.";
        return result;
    }
}

IngredientSingleResultDTO IngredientService::createIngredient(
    std::uint64_t memberId,
    const CreateIngredientRequestDTO &request)
{
    IngredientSingleResultDTO result;

    // memberId가 없으면 인증 실패로 처리한다.
    if (memberId == 0)
    {
        result.statusCode = 401;
        result.message = "Unauthorized.";
        return result;
    }

    // 필수 입력값은 앞뒤 공백을 제거한 뒤 검증한다.
    // storage는 대소문자 영향을 없애기 위해 소문자로 통일한다.
    const auto name = trim(request.name);
    const auto category = trim(request.category);
    const auto quantity = trim(request.quantity);
    const auto unit = trim(request.unit);
    const auto storage = toLower(trim(request.storage));
    const auto expiryDate = trim(request.expiryDate);

    // 이름, 카테고리, 수량, 단위, 보관방법, 유통기한은 모두 필수다.
    // trim 이후 빈 문자열이면 누락으로 판단한다.
    if (name.empty() || category.empty() || quantity.empty() || unit.empty() ||
        storage.empty() || expiryDate.empty())
    {
        result.statusCode = 400;
        result.message = "Required ingredient fields are missing.";
        return result;
    }

    // DB 컬럼 제약과 UI 검증 기준에 맞춰 문자열 길이를 제한한다.
    if (name.size() > 150U || category.size() > 50U || quantity.size() > 50U ||
        unit.size() > 20U)
    {
        result.statusCode = 400;
        result.message = "Ingredient field length exceeds limit.";
        return result;
    }

    // storage는 허용된 값(fridge/freezer/roomtemp/other)만 받는다.
    if (!isAllowedClientStorage(storage))
    {
        result.statusCode = 400;
        result.message = "Invalid storage value.";
        return result;
    }

    // 유통기한은 YYYY-MM-DD 형식만 허용한다.
    if (!isValidDate(expiryDate))
    {
        result.statusCode = 400;
        result.message = "Invalid expiry date format.";
        return result;
    }

    // 실제 저장 전 DTO를 정규화해 Mapper 계층으로 전달한다.
    // 공공 API 연계 필드는 공백 정리 후 빈 값을 null로 치환한다.
    CreateIngredientRequestDTO normalized = request;
    normalized.name = name;
    normalized.category = category;
    normalized.quantity = quantity;
    normalized.unit = unit;
    normalized.storage = toDbStorage(storage);
    normalized.expiryDate = expiryDate;

    // 선택 필드는 값이 있을 때만 trim하고 비면 nullopt로 정리한다.
    // 불필요한 공백 입력이 DB에 남지 않도록 사전 정리한다.
    if (normalized.publicFoodCode.has_value())
    {
        normalized.publicFoodCode = trim(*normalized.publicFoodCode);
        if (normalized.publicFoodCode->empty())
        {
            normalized.publicFoodCode = std::nullopt;
        }
    }
    if (normalized.nutritionBasisAmount.has_value())
    {
        normalized.nutritionBasisAmount = trim(*normalized.nutritionBasisAmount);
        if (normalized.nutritionBasisAmount->empty())
        {
            normalized.nutritionBasisAmount = std::nullopt;
        }
    }
    if (normalized.sourceName.has_value())
    {
        normalized.sourceName = trim(*normalized.sourceName);
        if (normalized.sourceName->empty())
        {
            normalized.sourceName = std::nullopt;
        }
    }
    if (normalized.manufacturerName.has_value())
    {
        normalized.manufacturerName = trim(*normalized.manufacturerName);
        if (normalized.manufacturerName->empty())
        {
            normalized.manufacturerName = std::nullopt;
        }
    }
    if (normalized.originCountryName.has_value())
    {
        normalized.originCountryName = trim(*normalized.originCountryName);
        if (normalized.originCountryName->empty())
        {
            normalized.originCountryName = std::nullopt;
        }
    }

    // 추가 정규화: importYn/dataBaseDate를 서버 규격으로 맞춘다.
    // importYn은 Y/N, dataBaseDate는 YYYYMMDD 입력도 허용해 날짜 형식으로 변환한다.
    normalized.importYn = normalizeImportYn(normalized.importYn);
    normalized.dataBaseDate = normalizeDataBaseDate(normalized.dataBaseDate);

    try
    {
        // 정규화된 DTO를 DB에 insert한다.
        auto created = mapper_.insertIngredient(memberId, normalized);
        if (!created.has_value())
        {
            result.statusCode = 500;
            result.message = "Failed to create ingredient.";
            return result;
        }

        // 생성 직후 응답 전용 형식으로 한 번 더 정규화한다.
        normalizeIngredientForClient(*created);

        result.ok = true;
        result.statusCode = 201;
        result.message = "Ingredient created.";
        result.ingredient = created;
        return result;
    }
    catch (const std::exception &)
    {
        result.statusCode = 500;
        result.message = "Server error while creating ingredient.";
        return result;
    }
}

IngredientSingleResultDTO IngredientService::updateIngredient(
    std::uint64_t memberId,
    std::uint64_t ingredientId,
    const UpdateIngredientRequestDTO &request)
{
    IngredientSingleResultDTO result;

    // memberId가 없으면 인증 실패로 처리한다.
    if (memberId == 0)
    {
        result.statusCode = 401;
        result.message = "Unauthorized.";
        return result;
    }

    // 수정 대상 id가 없으면 잘못된 요청이다.
    if (ingredientId == 0)
    {
        result.statusCode = 400;
        result.message = "Ingredient id is required.";
        return result;
    }

    // 수정 API도 생성 API와 동일한 검증 규칙을 적용한다.
    // storage는 소문자 정규화 후 허용 값만 통과시킨다.
    const auto name = trim(request.name);
    const auto category = trim(request.category);
    const auto quantity = trim(request.quantity);
    const auto unit = trim(request.unit);
    const auto storage = toLower(trim(request.storage));
    const auto expiryDate = trim(request.expiryDate);

    // 필수값 누락 여부를 다시 검사한다.
    // 공백만 들어온 값도 누락으로 본다.
    if (name.empty() || category.empty() || quantity.empty() || unit.empty() ||
        storage.empty() || expiryDate.empty())
    {
        result.statusCode = 400;
        result.message = "Required ingredient fields are missing.";
        return result;
    }

    // 문자열 길이 제한을 확인한다.
    if (name.size() > 150U || category.size() > 50U || quantity.size() > 50U ||
        unit.size() > 20U)
    {
        result.statusCode = 400;
        result.message = "Ingredient field length exceeds limit.";
        return result;
    }

    // storage 값 유효성을 확인한다.
    if (!isAllowedClientStorage(storage))
    {
        result.statusCode = 400;
        result.message = "Invalid storage value.";
        return result;
    }

    // 유통기한 형식을 확인한다.
    if (!isValidDate(expiryDate))
    {
        result.statusCode = 400;
        result.message = "Invalid expiry date format.";
        return result;
    }

    try
    {
        // 먼저 해당 회원 소유의 식재료가 존재하는지 확인한다.
        // 없으면 404를 반환해 다른 회원 데이터 접근을 막는다.
        const auto existing = mapper_.findByIdForMember(ingredientId, memberId);
        if (!existing.has_value())
        {
            result.statusCode = 404;
            result.message = "Ingredient not found.";
            return result;
        }

        // 업데이트용 DTO를 검증된 값으로 재구성한다.
        // 보관방법은 DB enum에 맞는 값으로 변환해 저장한다.
        UpdateIngredientRequestDTO normalized;
        normalized.name = name;
        normalized.category = category;
        normalized.quantity = quantity;
        normalized.unit = unit;
        normalized.storage = toDbStorage(storage);
        normalized.expiryDate = expiryDate;

        if (!mapper_.updateEditableFields(ingredientId, memberId, normalized))
        {
            result.statusCode = 500;
            result.message = "Failed to update ingredient.";
            return result;
        }

        // update 후 최신 값을 다시 조회한다.
        // 응답에는 정규화된 최신 레코드를 내려준다.
        auto updated = mapper_.findByIdForMember(ingredientId, memberId);
        if (!updated.has_value())
        {
            result.statusCode = 500;
            result.message = "Failed to load updated ingredient.";
            return result;
        }

        normalizeIngredientForClient(*updated);

        result.ok = true;
        result.statusCode = 200;
        result.message = "Ingredient updated.";
        result.ingredient = updated;
        return result;
    }
    catch (const std::exception &)
    {
        result.statusCode = 500;
        result.message = "Server error while updating ingredient.";
        return result;
    }
}

IngredientDeleteResultDTO IngredientService::deleteIngredient(
    std::uint64_t memberId,
    std::uint64_t ingredientId)
{
    IngredientDeleteResultDTO result;

    // memberId가 없으면 인증 실패로 처리한다.
    if (memberId == 0)
    {
        result.statusCode = 401;
        result.message = "Unauthorized.";
        return result;
    }

    // 삭제 대상 id가 없으면 잘못된 요청이다.
    if (ingredientId == 0)
    {
        result.statusCode = 400;
        result.message = "Ingredient id is required.";
        return result;
    }

    try
    {
        // 해당 회원 소유의 식재료인지 먼저 확인한다.
        // 없으면 삭제 대신 404를 반환한다.
        const auto existing = mapper_.findByIdForMember(ingredientId, memberId);
        if (!existing.has_value())
        {
            result.statusCode = 404;
            result.message = "Ingredient not found.";
            return result;
        }

        // 물리 삭제 대신 삭제 플래그만 갱신하는 소프트 삭제를 사용한다.
        // 실수 복구와 이력 추적을 위해 레코드는 유지한다.
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
    catch (const std::exception &)
    {
        result.statusCode = 500;
        result.message = "Server error while deleting ingredient.";
        return result;
    }
}

std::string IngredientService::trim(std::string value)
{
    // 문자열 앞뒤 공백을 제거한다.
    // "   "처럼 공백만 있는 입력을 빈 문자열로 만들 때 사용한다.
    const auto notSpace = [](unsigned char ch) { return !std::isspace(ch); };

    value.erase(value.begin(),
                std::find_if(value.begin(), value.end(), notSpace));
    value.erase(
        std::find_if(value.rbegin(), value.rend(), notSpace).base(), value.end());
    return value;
}

std::string IngredientService::toLower(std::string value)
{
    // 영문 문자열을 소문자로 통일한다.
    // storage, importYn 같은 코드값 비교 전에 정규화할 때 사용한다.
    std::transform(value.begin(), value.end(), value.begin(), [](unsigned char ch) {
        return static_cast<char>(std::tolower(ch));
    });
    return value;
}

bool IngredientService::isValidDate(const std::string &value)
{
    // 날짜가 YYYY-MM-DD 형식인지 정규식으로 확인한다.
    // 예: "2024-05-12"만 허용하며 달력 유효성(예: 02-30)은 별도 검증하지 않는다.
    static const std::regex pattern("^[0-9]{4}-[0-9]{2}-[0-9]{2}$");
    return std::regex_match(value, pattern);
}

bool IngredientService::isAllowedClientStorage(const std::string &value)
{
    // 클라이언트에서 허용하는 storage 값인지 검사한다.
    // 변환 규칙은 toDbStorage/toClientStorage와 반드시 동일해야 한다.
    return value == "fridge" || value == "freezer" || value == "roomtemp" ||
           value == "other";
}

std::string IngredientService::toDbStorage(const std::string &clientValue)
{
    // 클라이언트 storage 값을 DB enum 값으로 변환한다.
    // 알 수 없는 값은 OTHER로 매핑해 안전하게 처리한다.
    if (clientValue == "fridge")
    {
        return "FRIDGE";
    }
    if (clientValue == "freezer")
    {
        return "FREEZER";
    }
    if (clientValue == "roomtemp")
    {
        return "ROOM_TEMP";
    }
    return "OTHER";
}

std::string IngredientService::toClientStorage(const std::string &dbValue)
{
    // DB enum 값을 클라이언트 storage 값으로 변환한다.
    // 공백과 대소문자 변형을 대비해 trim/toLower 후 비교한다.
    const auto normalized = toLower(trim(dbValue));
    if (normalized == "fridge")
    {
        return "fridge";
    }
    if (normalized == "freezer")
    {
        return "freezer";
    }
    if (normalized == "room_temp")
    {
        return "roomTemp";
    }
    return "other";
}

std::optional<std::string> IngredientService::normalizeImportYn(
    const std::optional<std::string> &value)
{
    // importYn 값을 Y/N 표준값으로 정규화한다.
    // 빈 값이거나 허용되지 않은 값이면 nullopt로 처리한다.
    if (!value.has_value())
    {
        return std::nullopt;
    }

    const auto normalized = toLower(trim(*value));
    if (normalized == "y")
    {
        return "Y";
    }
    if (normalized == "n")
    {
        return "N";
    }
    return std::nullopt;
}

void IngredientService::normalizeIngredientForClient(IngredientDTO &ingredient)
{
    // DB에서 읽은 식재료 DTO를 클라이언트 응답 형식으로 정리한다.
    // storage, importYn, dataBaseDate를 API 스펙에 맞게 변환한다.
    ingredient.storage = toClientStorage(ingredient.storage);
    ingredient.importYn = normalizeImportYn(ingredient.importYn);
    ingredient.dataBaseDate = normalizeDataBaseDate(ingredient.dataBaseDate);
}

}  // ingredient 네임스페이스


