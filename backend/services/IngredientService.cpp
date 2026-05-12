#include "IngredientService.h"

#include <drogon/drogon.h>
#include <drogon/utils/Utilities.h>

#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <regex>
#include <utility>
#include <vector>

namespace
{

// JSON 값을 비어 있지 않은 선택 문자열로 변환한다.
std::optional<std::string> optionalJsonString(const Json::Value &value)
{
    // 공공 API 응답은 필드별 타입이 항상 일정하지 않다.
    // 이 함수는 문자열처럼 다룰 수 있는 값만 골라 이후 매핑 로직을 단순하게 만든다.
    if (value.isNull())
    {
        return std::nullopt;
    }

    if (value.isString())
    {
        const auto str = value.asString();
        if (str.empty())
        {
            return std::nullopt;
        }
        return str;
    }

    // 외부 API의 일부 필드는 숫자여도 문자열 id를 의미할 수 있다.
    if (value.isNumeric())
    {
        return value.asString();
    }

    return std::nullopt;
}

// 후보 키 중 첫 번째 비어 있지 않은 문자열 값을 반환한다.
std::optional<std::string> firstStringFromKeys(
    const Json::Value &row,
    std::initializer_list<const char *> keys)
{
    // 공공 API는 같은 의미의 필드도 camelCase, 대문자 스네이크 케이스 등으로 내려줄 수 있다.
    // 호출부는 가능한 후보 키를 넘기고, 여기서는 처음 발견된 유효 문자열을 선택한다.
    for (const auto *key : keys)
    {
        if (!row.isMember(key))
        {
            continue;
        }

        const auto value = optionalJsonString(row[key]);
        if (value.has_value() && !value->empty())
        {
            return value;
        }
    }

    return std::nullopt;
}

// JSON 값을 선택 실수값으로 변환한다.
std::optional<double> optionalJsonDouble(const Json::Value &value)
{
    // 영양 성분 값은 숫자 또는 문자열로 내려올 수 있으므로 두 형식을 모두 처리한다.
    // 파싱할 수 없는 값은 항목 전체 실패가 아니라 해당 영양값 누락으로 취급한다.
    if (value.isNull())
    {
        return std::nullopt;
    }

    try
    {
        if (value.isNumeric())
        {
            return value.asDouble();
        }

        if (value.isString())
        {
            auto text = value.asString();

            // 숫자 파싱 전에 공백과 쉼표 구분자를 제거한다.
            text.erase(
                std::remove_if(text.begin(), text.end(), [](unsigned char ch) {
                    return std::isspace(ch) != 0 || ch == ',';
                }),
                text.end());

            if (text.empty())
            {
                return std::nullopt;
            }

            return std::stod(text);
        }
    }
    catch (const std::exception &)
    {
        // 파싱 실패가 전체 응답 매핑을 중단하지 않도록 무시한다.
        return std::nullopt;
    }

    return std::nullopt;
}

// 후보 키 중 첫 번째로 파싱 가능한 숫자 값을 반환한다.
std::optional<double> firstDoubleFromKeys(const Json::Value &row,
                                          std::initializer_list<const char *> keys)
{
    // 문자열 필드와 마찬가지로 숫자 필드도 여러 후보 키를 순서대로 확인한다.
    for (const auto *key : keys)
    {
        if (!row.isMember(key))
        {
            continue;
        }

        const auto value = optionalJsonDouble(row[key]);
        if (value.has_value())
        {
            return value;
        }
    }

    return std::nullopt;
}

// API 날짜 문자열을 정규화한다.
// YYYYMMDD 형식을 지원하며 YYYY-MM-DD 형식으로 맞춘다.
std::optional<std::string> normalizeDataBaseDate(
    const std::optional<std::string> &value)
{
    // 공공 API 기준일자는 "20240512"처럼 붙어서 오기도 하고 이미 구분자가 있기도 하다.
    // 빈 값은 저장하지 않고, 8자리 숫자만 서비스 표준 날짜 형식으로 변환한다.
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

// 알려진 여러 API 응답 구조에서 행 배열을 추출한다.
std::vector<Json::Value> extractRows(const Json::Value &root)
{
    std::vector<Json::Value> out;

    // API 제공 방식이나 버전에 따라 목록이 data 바로 아래에 있을 수 있다.
    // 응답 구조 1: { "data": [...] }
    if (root.isMember("data") && root["data"].isArray())
    {
        for (const auto &item : root["data"])
        {
            out.push_back(item);
        }
        return out;
    }

    // 공공데이터포털 표준 응답처럼 response/body/items 안에 들어오는 경우도 처리한다.
    // 응답 구조 2: { "response": { "body": { "items": ... } } }
    if (root.isMember("response") && root["response"].isObject())
    {
        const auto &response = root["response"];
        if (response.isMember("body") && response["body"].isObject())
        {
            const auto &body = response["body"];
            if (body.isMember("items"))
            {
                const auto &items = body["items"];
                if (items.isArray())
                {
                    // items 자체가 배열이면 각 원소가 하나의 검색 결과다.
                    for (const auto &item : items)
                    {
                        out.push_back(item);
                    }
                    return out;
                }

                if (items.isObject() && items.isMember("item"))
                {
                    const auto &itemField = items["item"];
                    if (itemField.isArray())
                    {
                        // items.item이 배열인 일반적인 복수 결과 형태다.
                        for (const auto &item : itemField)
                        {
                            out.push_back(item);
                        }
                        return out;
                    }

                    if (itemField.isObject())
                    {
                        // 결과가 1건이면 배열 대신 단일 객체로 내려오는 경우가 있다.
                        out.push_back(itemField);
                        return out;
                    }
                }
            }
        }
    }

    return out;
}

}  // 익명 네임스페이스

namespace ingredient
{

IngredientListResultDTO IngredientService::getIngredients(std::uint64_t memberId)
{
    IngredientListResultDTO result;

    // 세션이 없는 요청은 인증되지 않은 요청으로 처리한다.
    if (memberId == 0)
    {
        result.statusCode = 401;
        result.message = "Unauthorized.";
        return result;
    }

    try
    {
        // 회원별 식재료 조회는 매퍼가 담당한다.
        // 서비스는 조회 결과를 클라이언트 응답 계약에 맞게 후처리한다.
        result.ingredients = mapper_.findIngredientsByMemberId(memberId);

        // 응답 직렬화 전에 열거형과 날짜 값을 클라이언트 형식으로 정규화한다.
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

    // 세션이 없는 요청은 인증되지 않은 요청으로 처리한다.
    if (memberId == 0)
    {
        result.statusCode = 401;
        result.message = "Unauthorized.";
        return result;
    }

    // 사용자가 직접 입력하는 핵심 필드는 먼저 공백을 제거한다.
    // storage는 대소문자를 허용하기 위해 소문자 기준으로 검증한다.
    const auto name = trim(request.name);
    const auto category = trim(request.category);
    const auto quantity = trim(request.quantity);
    const auto unit = trim(request.unit);
    const auto storage = toLower(trim(request.storage));
    const auto expiryDate = trim(request.expiryDate);

    // 이름, 카테고리, 수량, 단위, 보관 위치, 유통기한은 생성 시 반드시 필요하다.
    // 공백만 입력한 값은 trim 이후 빈 값으로 처리된다.
    if (name.empty() || category.empty() || quantity.empty() || unit.empty() ||
        storage.empty() || expiryDate.empty())
    {
        result.statusCode = 400;
        result.message = "Required ingredient fields are missing.";
        return result;
    }

    // DB 컬럼 크기와 UI 입력 제한을 맞추기 위해 문자열 길이를 서비스에서 방어한다.
    if (name.size() > 150U || category.size() > 50U || quantity.size() > 50U ||
        unit.size() > 20U)
    {
        result.statusCode = 400;
        result.message = "Ingredient field length exceeds limit.";
        return result;
    }

    // 보관 위치 값은 허용된 클라이언트 열거형이어야 한다.
    if (!isAllowedClientStorage(storage))
    {
        result.statusCode = 400;
        result.message = "Invalid storage value.";
        return result;
    }

    // 날짜는 YYYY-MM-DD 형식이어야 한다.
    if (!isValidDate(expiryDate))
    {
        result.statusCode = 400;
        result.message = "Invalid expiry date format.";
        return result;
    }

    // 원본 request를 복사한 뒤, 검증된 핵심 필드만 정규화된 값으로 덮어쓴다.
    // 이렇게 하면 공공 API 메타데이터 같은 선택 필드는 유지하면서도 핵심 필드는 신뢰할 수 있다.
    CreateIngredientRequestDTO normalized = request;
    normalized.name = name;
    normalized.category = category;
    normalized.quantity = quantity;
    normalized.unit = unit;
    normalized.storage = toDbStorage(storage);
    normalized.expiryDate = expiryDate;

    // 공공 API에서 가져온 선택 메타데이터는 사용자가 직접 입력하지 않을 수도 있다.
    // 공백 문자열을 저장하지 않도록 각 필드를 trim 후 비어 있으면 nullopt로 바꾼다.
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

    // 제한된 값 집합을 갖는 선택 필드를 정규화한다.
    // importYn은 Y/N만 허용하고, 기준일자는 YYYYMMDD 형식을 서비스 날짜 형식으로 맞춘다.
    normalized.importYn = normalizeImportYn(normalized.importYn);
    normalized.dataBaseDate = normalizeDataBaseDate(normalized.dataBaseDate);

    try
    {
        // 매퍼는 이미 정규화된 값을 받아 DB insert만 수행한다.
        auto created = mapper_.insertIngredient(memberId, normalized);
        if (!created.has_value())
        {
            result.statusCode = 500;
            result.message = "Failed to create ingredient.";
            return result;
        }

        // 생성 직후 응답도 목록 조회와 같은 클라이언트 형식으로 맞춘다.
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

    // 세션이 없는 요청은 인증되지 않은 요청으로 처리한다.
    if (memberId == 0)
    {
        result.statusCode = 401;
        result.message = "Unauthorized.";
        return result;
    }

    // 대상 id는 필수다.
    if (ingredientId == 0)
    {
        result.statusCode = 400;
        result.message = "Ingredient id is required.";
        return result;
    }

    // 수정 API는 생성 API와 같은 핵심 필드 세트를 받는다.
    // 공백 제거와 storage 소문자 변환을 먼저 적용해 검증 기준을 통일한다.
    const auto name = trim(request.name);
    const auto category = trim(request.category);
    const auto quantity = trim(request.quantity);
    const auto unit = trim(request.unit);
    const auto storage = toLower(trim(request.storage));
    const auto expiryDate = trim(request.expiryDate);

    // 현재 수정은 부분 업데이트가 아니라 전체 수정 방식이다.
    // 따라서 생성 때와 같은 필수 필드를 모두 요구한다.
    if (name.empty() || category.empty() || quantity.empty() || unit.empty() ||
        storage.empty() || expiryDate.empty())
    {
        result.statusCode = 400;
        result.message = "Required ingredient fields are missing.";
        return result;
    }

    // 문자열 필드 길이 제한을 확인한다.
    if (name.size() > 150U || category.size() > 50U || quantity.size() > 50U ||
        unit.size() > 20U)
    {
        result.statusCode = 400;
        result.message = "Ingredient field length exceeds limit.";
        return result;
    }

    // 보관 위치 값은 허용된 클라이언트 열거형이어야 한다.
    if (!isAllowedClientStorage(storage))
    {
        result.statusCode = 400;
        result.message = "Invalid storage value.";
        return result;
    }

    // 날짜는 YYYY-MM-DD 형식이어야 한다.
    if (!isValidDate(expiryDate))
    {
        result.statusCode = 400;
        result.message = "Invalid expiry date format.";
        return result;
    }

    try
    {
        // 대상 식재료는 현재 회원의 소유여야 한다.
        // 이 확인으로 다른 회원의 식재료 id를 직접 호출하는 요청을 차단한다.
        const auto existing = mapper_.findByIdForMember(ingredientId, memberId);
        if (!existing.has_value())
        {
            result.statusCode = 404;
            result.message = "Ingredient not found.";
            return result;
        }

        // 수정 가능한 필드만 별도 DTO에 담는다.
        // 공공 API에서 유래한 코드/영양 원본 정보는 여기서 덮어쓰지 않는다.
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

        // update 결과만으로는 DB 트리거나 기본값 반영 여부를 알기 어렵다.
        // 응답에 실제 저장된 최신 값을 담기 위해 다시 조회한다.
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

    // 세션이 없는 요청은 인증되지 않은 요청으로 처리한다.
    if (memberId == 0)
    {
        result.statusCode = 401;
        result.message = "Unauthorized.";
        return result;
    }

    // 대상 id는 필수다.
    if (ingredientId == 0)
    {
        result.statusCode = 400;
        result.message = "Ingredient id is required.";
        return result;
    }

    try
    {
        // 대상 식재료는 현재 회원의 소유여야 한다.
        // 삭제 역시 소유권을 먼저 확인해 id 추측 공격을 막는다.
        const auto existing = mapper_.findByIdForMember(ingredientId, memberId);
        if (!existing.has_value())
        {
            result.statusCode = 404;
            result.message = "Ingredient not found.";
            return result;
        }

        // 실제 row를 지우지 않고 삭제 상태만 표시한다.
        // 추후 복구, 감사 로그, 통계 계산에 원본 데이터를 남길 수 있다.
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

void IngredientService::searchProcessedFoods(const std::string &keyword,
                                             ProcessedFoodSearchCallback &&callback)
{
    ProcessedFoodSearchResultDTO invalidInput;
    const auto normalizedKeyword = trim(keyword);

    // 검색 키워드는 필수다.
    // 공백만 입력한 요청은 외부 API를 호출하지 않고 바로 400으로 끝낸다.
    if (normalizedKeyword.empty())
    {
        invalidInput.statusCode = 400;
        invalidInput.message = "Keyword is required.";
        callback(std::move(invalidInput));
        return;
    }

    // 공공 API 키는 환경변수 또는 로컬 설정 파일에서 가져올 수 있다.
    // 배포 환경은 환경변수를 우선하고, 로컬 개발 환경은 keys.local.json을 보조로 사용한다.
    auto serviceKey = getEnvValue("PUBLIC_NUTRI_SERVICE_KEY");
    if (!serviceKey.has_value())
    {
        serviceKey = getServiceKeyFromLocalFile();
    }

    if (!serviceKey.has_value())
    {
        // API 키가 없으면 외부 호출 자체가 불가능하므로 서버 설정 오류로 반환한다.
        ProcessedFoodSearchResultDTO keyError;
        keyError.statusCode = 500;
        keyError.message =
            "PUBLIC_NUTRI_SERVICE_KEY is not configured. "
            "Set env var or backend/config/keys.local.json.";
        callback(std::move(keyError));
        return;
    }

    // API 키와 검색어를 URL 인코딩해 요청 경로를 만든다.
    // 키나 검색어에 특수문자가 있어도 쿼리 문자열이 깨지지 않게 한다.
    const auto path =
        "/openapi/tn_pubr_public_nutri_process_info_api"
        "?serviceKey=" +
        drogon::utils::urlEncodeComponent(*serviceKey) +
        "&type=json&pageNo=1&numOfRows=10&foodNm=" +
        drogon::utils::urlEncodeComponent(normalizedKeyword);

    auto client = drogon::HttpClient::newHttpClient("https://api.data.go.kr");
    auto request = drogon::HttpRequest::newHttpRequest();
    request->setMethod(drogon::Get);
    request->setPath(path);

    // 외부 API를 비동기로 호출하고 응답 본문을 DTO 목록으로 매핑해 콜백으로 반환한다.
    // 이 함수는 즉시 반환되고, 실제 결과는 람다 내부에서 callback으로 전달된다.
    client->sendRequest(
        request,
        [callback = std::move(callback)](drogon::ReqResult reqResult,
                                         const drogon::HttpResponsePtr &response) mutable {
            ProcessedFoodSearchResultDTO result;

            // 네트워크 또는 프로토콜 수준 실패를 처리한다.
            if (reqResult != drogon::ReqResult::Ok || !response)
            {
                // API 서버에 도달하지 못했거나 Drogon이 응답 객체를 만들지 못한 경우다.
                result.statusCode = 502;
                result.message = "Failed to call nutrition public API.";
                callback(std::move(result));
                return;
            }

            // 외부 API의 200이 아닌 응답을 처리한다.
            if (response->statusCode() != drogon::k200OK)
            {
                // 외부 API 장애나 인증 오류는 우리 서비스 관점에서 bad gateway로 전달한다.
                result.statusCode = 502;
                result.message = "Nutrition public API returned error.";
                callback(std::move(result));
                return;
            }

            // 응답 본문은 유효한 JSON이어야 한다.
            const auto body = response->getJsonObject();
            if (!body)
            {
                // HTTP는 성공했지만 JSON 파싱이 안 되면 외부 응답 형식 오류로 본다.
                result.statusCode = 502;
                result.message = "Invalid response from nutrition public API.";
                callback(std::move(result));
                return;
            }

            const auto rows = extractRows(*body);
            result.foods.reserve(rows.size());

            for (const auto &row : rows)
            {
                // 각 결과 항목에는 식품 코드와 식품명이 필수다.
                // 둘 중 하나라도 없으면 프론트에서 선택 가능한 식품으로 쓰기 어렵기 때문에 제외한다.
                auto foodCode =
                    firstStringFromKeys(row, {"foodCd", "FOOD_CD"});
                auto foodName =
                    firstStringFromKeys(row, {"foodNm", "FOOD_NM"});
                if (!foodCode.has_value() || !foodName.has_value())
                {
                    continue;
                }

                ProcessedFoodSearchItemDTO dto;
                dto.foodCode = *foodCode;
                dto.foodName = *foodName;
                // 식품 분류는 가장 상세한 단계부터 후보 키를 확인한다.
                dto.foodGroupName = firstStringFromKeys(
                    row,
                    {"foodLv7Nm", "foodLv6Nm", "foodLv5Nm", "foodLv4Nm",
                     "foodLv3Nm", "FOOD_LV7_NM", "FOOD_LV6_NM", "FOOD_LV5_NM",
                     "FOOD_LV4_NM", "FOOD_LV3_NM"});

                // 주요 영양 성분 값을 매핑한다.
                // 값이 없거나 숫자로 파싱되지 않는 항목은 nullopt로 남긴다.
                dto.nutritionBasisAmount = firstStringFromKeys(
                    row, {"nutConSrtrQua", "NUT_CON_SRTR_QUA"});
                dto.energyKcal = firstDoubleFromKeys(row, {"enerc", "ENERC"});
                dto.proteinG = firstDoubleFromKeys(row, {"prot", "PROT"});
                dto.fatG = firstDoubleFromKeys(row, {"fatce", "FATCE"});
                dto.carbohydrateG =
                    firstDoubleFromKeys(row, {"chocdf", "CHOCDF"});
                dto.sugarG = firstDoubleFromKeys(row, {"sugar", "SUGAR"});
                dto.sodiumMg = firstDoubleFromKeys(row, {"nat", "NAT"});

                // 원산지와 출처 관련 부가 메타데이터를 매핑한다.
                // 이 값들은 식재료 생성 시 선택 메타데이터로 함께 저장될 수 있다.
                dto.sourceName = firstStringFromKeys(row, {"srcNm", "SRC_NM"});
                dto.manufacturerName =
                    firstStringFromKeys(row, {"mfrNm", "MFR_NM"});
                dto.importYn = normalizeImportYn(
                    firstStringFromKeys(row, {"imptYn", "IMPT_YN"}));
                dto.originCountryName =
                    firstStringFromKeys(row, {"cooNm", "COO_NM"});
                dto.dataBaseDate = normalizeDataBaseDate(
                    firstStringFromKeys(row, {"crtrYmd", "CRTR_YMD"}));

                result.foods.push_back(std::move(dto));
            }

            // 유효한 row가 하나도 없어도 검색 자체는 성공한 것으로 응답한다.
            result.ok = true;
            result.statusCode = 200;
            result.message = "Nutrition foods loaded.";
            callback(std::move(result));
        },
        10.0);
}

std::string IngredientService::trim(std::string value)
{
    // 문자열 앞뒤 공백을 제거하는 공용 유틸리티다.
    // 필수값 검증 전에 호출해서 "   " 같은 입력을 빈 값으로 판단하게 한다.
    const auto notSpace = [](unsigned char ch) { return !std::isspace(ch); };

    value.erase(value.begin(),
                std::find_if(value.begin(), value.end(), notSpace));
    value.erase(
        std::find_if(value.rbegin(), value.rend(), notSpace).base(), value.end());
    return value;
}

std::string IngredientService::toLower(std::string value)
{
    // 대소문자 구분 없는 비교를 위해 정규화한다.
    // storage, importYn 같은 enum성 문자열 비교에서 사용한다.
    std::transform(value.begin(), value.end(), value.begin(), [](unsigned char ch) {
        return static_cast<char>(std::tolower(ch));
    });
    return value;
}

bool IngredientService::isValidDate(const std::string &value)
{
    // API 입력 날짜 형식을 엄격하게 확인한다.
    // 여기서는 "2024-05-12" 같은 문자열 모양만 확인하고 실제 존재 날짜 검증은 하지 않는다.
    static const std::regex pattern("^[0-9]{4}-[0-9]{2}-[0-9]{2}$");
    return std::regex_match(value, pattern);
}

bool IngredientService::isAllowedClientStorage(const std::string &value)
{
    // 클라이언트에서 지원하는 보관 위치 열거형 목록이다.
    // 새 보관 위치가 추가되면 여기와 toDbStorage/toClientStorage를 함께 갱신해야 한다.
    return value == "fridge" || value == "freezer" || value == "roomtemp" ||
           value == "other";
}

std::string IngredientService::toDbStorage(const std::string &clientValue)
{
    // 클라이언트 열거형 값을 DB 열거형 값으로 변환한다.
    // 컨트롤러와 프론트엔드는 소문자 값을 쓰고, DB는 대문자 enum 값을 쓴다.
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
    // DB 열거형 값을 클라이언트 열거형 값으로 되돌린다.
    // DB에서 이미 소문자 형태로 내려오는 경우도 대비해 trim/toLower를 먼저 적용한다.
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
    // 수입 여부 플래그는 Y/N만 남기도록 정규화한다.
    // 알 수 없는 값은 잘못된 문자열을 저장하지 않고 값 없음으로 처리한다.
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
    // 매퍼 결과 필드를 클라이언트 계약에 맞게 정규화한다.
    // 이 함수를 거친 DTO만 컨트롤러 응답으로 나가도록 유지하면 표현 변환이 한곳에 모인다.
    ingredient.storage = toClientStorage(ingredient.storage);
    ingredient.importYn = normalizeImportYn(ingredient.importYn);
    ingredient.dataBaseDate = normalizeDataBaseDate(ingredient.dataBaseDate);
}

std::optional<std::string> IngredientService::getEnvValue(const char *key)
{
    // 비어 있지 않은 환경변수 값만 반환한다.
    // 비어 있는 문자열은 설정되지 않은 것과 동일하게 취급한다.
    const auto *raw = std::getenv(key);
    if (raw == nullptr)
    {
        return std::nullopt;
    }

    const std::string value(raw);
    if (value.empty())
    {
        return std::nullopt;
    }
    return value;
}

std::optional<std::string> IngredientService::getServiceKeyFromLocalFile()
{
    // 파일 시스템을 반복해서 읽지 않도록 조회 결과를 한 번만 캐시한다.
    // 실패 결과도 loaded=true로 기억해서 요청마다 같은 파일들을 반복 탐색하지 않는다.
    static bool loaded = false;
    static std::optional<std::string> cached;
    if (loaded)
    {
        return cached;
    }
    loaded = true;

    // 실행 위치가 달라도 찾을 수 있도록 가능한 설정 파일 경로를 순서대로 시도한다.
    // 서버를 프로젝트 루트, backend 폴더, 빌드 폴더 어디에서 실행해도 로컬 키를 찾기 위한 방어다.
    const std::vector<std::filesystem::path> candidates = {
        std::filesystem::path("config") / "keys.local.json",
        std::filesystem::path("backend") / "config" / "keys.local.json",
        std::filesystem::path("..") / "config" / "keys.local.json",
        std::filesystem::path("..") / ".." / "config" / "keys.local.json",
        std::filesystem::path("..") / ".." / ".." / "config" / "keys.local.json",
    };

    for (const auto &path : candidates)
    {
        // 존재하지 않는 후보 경로는 건너뛴다.
        if (!std::filesystem::exists(path))
        {
            continue;
        }

        // JSON 설정 파일을 열고 파싱한다.
        std::ifstream file(path);
        if (!file.is_open())
        {
            continue;
        }

        Json::Value root;
        Json::CharReaderBuilder builder;
        std::string errors;
        if (!Json::parseFromStream(builder, file, &root, &errors))
        {
            // 깨진 JSON 파일은 무시하고 다음 후보 경로를 확인한다.
            continue;
        }

        // publicNutriServiceKey 키가 있는지 확인한다.
        if (!root.isObject() || !root["publicNutriServiceKey"].isString())
        {
            // 파일은 있어도 기대한 키가 없으면 다른 후보를 계속 확인한다.
            continue;
        }

        // 공백 제거 후 비어 있지 않은 키만 사용한다.
        auto value = trim(root["publicNutriServiceKey"].asString());
        if (value.empty())
        {
            continue;
        }

        // 성공한 조회 결과를 캐시한다.
        cached = value;
        return cached;
    }

    // 모든 후보 경로에서 키를 찾지 못한 경우다.
    return std::nullopt;
}

}  // 식재료 네임스페이스
