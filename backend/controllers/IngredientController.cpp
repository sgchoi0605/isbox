#include "IngredientController.h"

#include <algorithm>
#include <cctype>
#include <string>
#include <utility>

namespace
{

// 공통 에러 응답 JSON 본문을 생성한다.
Json::Value makeErrorBody(const std::string &message)
{
    Json::Value body;
    body["ok"] = false;
    body["message"] = message;
    return body;
}

// JSON의 문자열 필드를 optional<string> 형태로 안전하게 읽는다.
std::optional<std::string> readOptionalString(const Json::Value &json,
                                              const char *key)
{
    // 키가 없으면 값이 없는 것으로 처리한다.
    if (!json.isMember(key))
    {
        return std::nullopt;
    }

    // 값 참조를 가져온다.
    const auto &value = json[key];

    // null은 값이 없는 것으로 처리한다.
    if (value.isNull())
    {
        return std::nullopt;
    }

    // 문자열 타입이 아니면 잘못된 값으로 보고 무시한다.
    if (!value.isString())
    {
        return std::nullopt;
    }

    // 빈 문자열도 미입력 값으로 처리한다.
    const auto text = value.asString();
    if (text.empty())
    {
        return std::nullopt;
    }

    // 유효한 문자열 값을 반환한다.
    return text;
}

// JSON의 숫자/문자열 숫자 필드를 optional<double> 형태로 안전하게 읽는다.
std::optional<double> readOptionalDouble(const Json::Value &json, const char *key)
{
    // 키가 없으면 값이 없는 것으로 처리한다.
    if (!json.isMember(key))
    {
        return std::nullopt;
    }

    // 값 참조를 가져온다.
    const auto &value = json[key];

    // null은 값이 없는 것으로 처리한다.
    if (value.isNull())
    {
        return std::nullopt;
    }

    try
    {
        // JSON 숫자 타입이면 그대로 double로 변환한다.
        if (value.isNumeric())
        {
            return value.asDouble();
        }

        // JSON 문자열 타입이면 숫자 파싱을 시도한다.
        if (value.isString())
        {
            auto text = value.asString();

            // 공백과 천 단위 구분 쉼표를 제거해 파싱 오차를 줄인다.
            text.erase(std::remove_if(text.begin(), text.end(), [](unsigned char ch) {
                           return std::isspace(ch) != 0 || ch == ',';
                       }),
                       text.end());

            // 정리 후 빈 값이면 미입력으로 처리한다.
            if (text.empty())
            {
                return std::nullopt;
            }

            // 문자열 숫자를 double로 변환한다.
            return std::stod(text);
        }
    }
    catch (const std::exception &)
    {
        // 파싱 실패 시 값이 없는 것으로 처리한다.
        return std::nullopt;
    }

    // 숫자/문자열이 아닌 타입은 무시한다.
    return std::nullopt;
}

}  // namespace

namespace ingredient
{

// 식재료 관련 API 엔드포인트를 HTTP 메서드별로 등록한다.
void IngredientController::registerHandlers()
{
    // 전역 Drogon 앱 인스턴스를 가져온다.
    auto &app = drogon::app();

    // GET /api/ingredients: 식재료 목록 조회
    app.registerHandler(
        "/api/ingredients",
        [this](const drogon::HttpRequestPtr &request, Callback &&callback) {
            handleListIngredients(request, std::move(callback));
        },
        {drogon::Get});

    // POST /api/ingredients: 식재료 등록
    app.registerHandler(
        "/api/ingredients",
        [this](const drogon::HttpRequestPtr &request, Callback &&callback) {
            handleCreateIngredient(request, std::move(callback));
        },
        {drogon::Post});

    // PUT /api/ingredients: 식재료 수정
    app.registerHandler(
        "/api/ingredients",
        [this](const drogon::HttpRequestPtr &request, Callback &&callback) {
            handleUpdateIngredient(request, std::move(callback));
        },
        {drogon::Put});

    // DELETE /api/ingredients: 식재료 삭제
    app.registerHandler(
        "/api/ingredients",
        [this](const drogon::HttpRequestPtr &request, Callback &&callback) {
            handleDeleteIngredient(request, std::move(callback));
        },
        {drogon::Delete});

    // GET /api/nutrition/processed-foods: 가공식품 검색
    app.registerHandler(
        "/api/nutrition/processed-foods",
        [this](const drogon::HttpRequestPtr &request, Callback &&callback) {
            handleSearchProcessedFoods(request, std::move(callback));
        },
        {drogon::Get});
}

// 식재료 DTO를 응답용 JSON 객체로 변환한다.
Json::Value IngredientController::buildIngredientJson(
    const IngredientDTO &ingredient)
{
    Json::Value item;

    // 문자열 ID와 숫자 ID를 함께 제공해 프론트엔드 호환성을 유지한다.
    item["id"] = std::to_string(ingredient.ingredientId);
    item["ingredientId"] = static_cast<Json::UInt64>(ingredient.ingredientId);
    item["memberId"] = static_cast<Json::UInt64>(ingredient.memberId);

    // 선택 필드는 값이 있을 때만 응답에 포함한다.
    if (ingredient.publicFoodCode.has_value())
    {
        item["publicFoodCode"] = *ingredient.publicFoodCode;
    }

    // 기본 필드를 그대로 매핑한다.
    item["name"] = ingredient.name;
    item["category"] = ingredient.category;
    item["quantity"] = ingredient.quantity;
    item["unit"] = ingredient.unit;
    item["storage"] = ingredient.storage;
    item["expiryDate"] = ingredient.expiryDate;

    // 영양 정보 필드는 값이 있을 때만 포함한다.
    if (ingredient.nutritionBasisAmount.has_value())
    {
        item["nutritionBasisAmount"] = *ingredient.nutritionBasisAmount;
    }
    if (ingredient.energyKcal.has_value())
    {
        item["energyKcal"] = *ingredient.energyKcal;
    }
    if (ingredient.proteinG.has_value())
    {
        item["proteinG"] = *ingredient.proteinG;
    }
    if (ingredient.fatG.has_value())
    {
        item["fatG"] = *ingredient.fatG;
    }
    if (ingredient.carbohydrateG.has_value())
    {
        item["carbohydrateG"] = *ingredient.carbohydrateG;
    }
    if (ingredient.sugarG.has_value())
    {
        item["sugarG"] = *ingredient.sugarG;
    }
    if (ingredient.sodiumMg.has_value())
    {
        item["sodiumMg"] = *ingredient.sodiumMg;
    }

    // 출처/제조사/원산지 관련 선택 필드를 포함한다.
    if (ingredient.sourceName.has_value())
    {
        item["sourceName"] = *ingredient.sourceName;
    }
    if (ingredient.manufacturerName.has_value())
    {
        item["manufacturerName"] = *ingredient.manufacturerName;
    }
    if (ingredient.importYn.has_value())
    {
        item["importYn"] = *ingredient.importYn;
    }
    if (ingredient.originCountryName.has_value())
    {
        item["originCountryName"] = *ingredient.originCountryName;
    }
    if (ingredient.dataBaseDate.has_value())
    {
        item["dataBaseDate"] = *ingredient.dataBaseDate;
    }

    // 생성/수정 시각을 포함한다.
    item["createdAt"] = ingredient.createdAt;
    item["updatedAt"] = ingredient.updatedAt;
    return item;
}

// 가공식품 검색 DTO를 응답용 JSON 객체로 변환한다.
Json::Value IngredientController::buildProcessedFoodJson(
    const ProcessedFoodSearchItemDTO &food)
{
    Json::Value item;

    // 검색 결과의 식품 코드와 이름
    item["foodCode"] = food.foodCode;
    item["foodName"] = food.foodName;

    // 선택 필드는 값이 있을 때만 응답에 포함한다.
    if (food.foodGroupName.has_value())
    {
        item["foodGroupName"] = *food.foodGroupName;
    }

    // 영양 정보 필드는 값이 있을 때만 포함한다.
    if (food.nutritionBasisAmount.has_value())
    {
        item["nutritionBasisAmount"] = *food.nutritionBasisAmount;
    }
    if (food.energyKcal.has_value())
    {
        item["energyKcal"] = *food.energyKcal;
    }
    if (food.proteinG.has_value())
    {
        item["proteinG"] = *food.proteinG;
    }
    if (food.fatG.has_value())
    {
        item["fatG"] = *food.fatG;
    }
    if (food.carbohydrateG.has_value())
    {
        item["carbohydrateG"] = *food.carbohydrateG;
    }
    if (food.sugarG.has_value())
    {
        item["sugarG"] = *food.sugarG;
    }
    if (food.sodiumMg.has_value())
    {
        item["sodiumMg"] = *food.sodiumMg;
    }

    // 메타 정보(출처/제조사/원산지/DB 기준일)를 포함한다.
    if (food.sourceName.has_value())
    {
        item["sourceName"] = *food.sourceName;
    }
    if (food.manufacturerName.has_value())
    {
        item["manufacturerName"] = *food.manufacturerName;
    }
    if (food.importYn.has_value())
    {
        item["importYn"] = *food.importYn;
    }
    if (food.originCountryName.has_value())
    {
        item["originCountryName"] = *food.originCountryName;
    }
    if (food.dataBaseDate.has_value())
    {
        item["dataBaseDate"] = *food.dataBaseDate;
    }

    return item;
}

// 요청 Origin 기준으로 CORS 응답 헤더를 추가한다.
void IngredientController::applyCors(const drogon::HttpRequestPtr &request,
                                     const drogon::HttpResponsePtr &response)
{
    // 브라우저가 보낸 Origin 헤더를 읽는다(대소문자 모두 대응).
    std::string origin = request->getHeader("Origin");
    if (origin.empty())
    {
        origin = request->getHeader("origin");
    }

    // Origin이 있으면 해당 Origin만 허용한다.
    if (!origin.empty())
    {
        response->addHeader("Access-Control-Allow-Origin", origin);
        response->addHeader("Vary", "Origin");
    }

    // 인증 쿠키/헤더 사용과 허용 메서드/헤더를 명시한다.
    response->addHeader("Access-Control-Allow-Credentials", "true");
    response->addHeader("Access-Control-Allow-Headers",
                        "Content-Type, X-Member-Id");
    response->addHeader("Access-Control-Allow-Methods",
                        "GET, POST, PUT, DELETE, OPTIONS");
}

// X-Member-Id 헤더를 숫자 멤버 ID로 안전하게 파싱한다.
std::optional<std::uint64_t> IngredientController::extractMemberIdHeader(
    const drogon::HttpRequestPtr &request)
{
    // 표준/소문자 헤더명을 모두 시도한다.
    auto memberIdValue = request->getHeader("X-Member-Id");
    if (memberIdValue.empty())
    {
        memberIdValue = request->getHeader("x-member-id");
    }

    // 헤더가 없으면 인증 정보가 없는 것으로 처리한다.
    if (memberIdValue.empty())
    {
        return std::nullopt;
    }

    try
    {
        // 양의 정수 ID인지 확인한다.
        const auto parsed = std::stoull(memberIdValue);
        if (parsed == 0)
        {
            return std::nullopt;
        }
        return parsed;
    }
    catch (const std::exception &)
    {
        // 숫자 파싱 실패 시 인증 오류로 처리한다.
        return std::nullopt;
    }
}

// 쿼리 파라미터 ingredientId를 숫자 ID로 안전하게 파싱한다.
std::optional<std::uint64_t> IngredientController::extractIngredientIdParameter(
    const drogon::HttpRequestPtr &request)
{
    // 요청의 ingredientId 파라미터를 읽는다.
    const auto value = request->getParameter("ingredientId");
    if (value.empty())
    {
        return std::nullopt;
    }

    try
    {
        // 0은 유효하지 않은 ID로 간주한다.
        const auto parsed = std::stoull(value);
        if (parsed == 0)
        {
            return std::nullopt;
        }
        return parsed;
    }
    catch (const std::exception &)
    {
        // 숫자 파싱 실패 시 잘못된 요청으로 처리한다.
        return std::nullopt;
    }
}

// 식재료 목록 조회 요청을 처리한다.
void IngredientController::handleListIngredients(
    const drogon::HttpRequestPtr &request,
    Callback &&callback)
{
    // 목록 조회는 로그인 사용자 식별이 필요하다.
    const auto memberId = extractMemberIdHeader(request);
    if (!memberId.has_value())
    {
        auto response =
            drogon::HttpResponse::newHttpJsonResponse(makeErrorBody("Unauthorized."));
        response->setStatusCode(drogon::k401Unauthorized);
        applyCors(request, response);
        callback(response);
        return;
    }

    // 서비스 계층에서 사용자 식재료 목록을 조회한다.
    const auto result = ingredientService_.getIngredients(*memberId);

    // 공통 응답 필드를 구성한다.
    Json::Value body;
    body["ok"] = result.ok;
    body["message"] = result.message;

    // 조회 결과를 JSON 배열로 직렬화한다.
    Json::Value items(Json::arrayValue);
    for (const auto &ingredient : result.ingredients)
    {
        items.append(buildIngredientJson(ingredient));
    }
    body["ingredients"] = items;

    // JSON 응답을 만들고 상태코드를 반영한다.
    auto response = drogon::HttpResponse::newHttpJsonResponse(body);
    response->setStatusCode(static_cast<drogon::HttpStatusCode>(result.statusCode));

    // CORS 헤더 적용 후 응답한다.
    applyCors(request, response);
    callback(response);
}

// 식재료 등록 요청을 처리한다.
void IngredientController::handleCreateIngredient(
    const drogon::HttpRequestPtr &request,
    Callback &&callback)
{
    // 등록 요청은 로그인 사용자 식별이 필요하다.
    const auto memberId = extractMemberIdHeader(request);
    if (!memberId.has_value())
    {
        auto response =
            drogon::HttpResponse::newHttpJsonResponse(makeErrorBody("Unauthorized."));
        response->setStatusCode(drogon::k401Unauthorized);
        applyCors(request, response);
        callback(response);
        return;
    }

    // 본문이 유효한 JSON인지 확인한다.
    const auto json = request->getJsonObject();
    if (!json)
    {
        auto response =
            drogon::HttpResponse::newHttpJsonResponse(makeErrorBody("Invalid JSON body."));
        response->setStatusCode(drogon::k400BadRequest);
        applyCors(request, response);
        callback(response);
        return;
    }

    // JSON을 생성 DTO로 매핑한다.
    CreateIngredientRequestDTO dto;
    dto.name = json->get("name", "").asString();
    dto.category = json->get("category", "").asString();
    dto.quantity = json->get("quantity", "").asString();
    dto.unit = json->get("unit", "").asString();
    dto.storage = json->get("storage", "").asString();
    dto.expiryDate = json->get("expiryDate", "").asString();

    // 선택 필드는 helper로 안전하게 읽어 optional로 옮긴다.
    dto.publicFoodCode = readOptionalString(*json, "publicFoodCode");
    dto.nutritionBasisAmount = readOptionalString(*json, "nutritionBasisAmount");
    dto.energyKcal = readOptionalDouble(*json, "energyKcal");
    dto.proteinG = readOptionalDouble(*json, "proteinG");
    dto.fatG = readOptionalDouble(*json, "fatG");
    dto.carbohydrateG = readOptionalDouble(*json, "carbohydrateG");
    dto.sugarG = readOptionalDouble(*json, "sugarG");
    dto.sodiumMg = readOptionalDouble(*json, "sodiumMg");
    dto.sourceName = readOptionalString(*json, "sourceName");
    dto.manufacturerName = readOptionalString(*json, "manufacturerName");
    dto.importYn = readOptionalString(*json, "importYn");
    dto.originCountryName = readOptionalString(*json, "originCountryName");
    dto.dataBaseDate = readOptionalString(*json, "dataBaseDate");

    // 생성 로직은 서비스 계층에 위임한다.
    const auto result = ingredientService_.createIngredient(*memberId, dto);

    // 공통 응답 필드를 구성한다.
    Json::Value body;
    body["ok"] = result.ok;
    body["message"] = result.message;

    // 생성 결과가 있으면 응답에 포함한다.
    if (result.ingredient.has_value())
    {
        body["ingredient"] = buildIngredientJson(*result.ingredient);
    }

    // JSON 응답 생성 후 상태코드를 반영한다.
    auto response = drogon::HttpResponse::newHttpJsonResponse(body);
    response->setStatusCode(static_cast<drogon::HttpStatusCode>(result.statusCode));

    // CORS 헤더 적용 후 응답한다.
    applyCors(request, response);
    callback(response);
}

// 식재료 수정 요청을 처리한다.
void IngredientController::handleUpdateIngredient(
    const drogon::HttpRequestPtr &request,
    Callback &&callback)
{
    // 수정 요청은 로그인 사용자 식별이 필요하다.
    const auto memberId = extractMemberIdHeader(request);
    if (!memberId.has_value())
    {
        auto response =
            drogon::HttpResponse::newHttpJsonResponse(makeErrorBody("Unauthorized."));
        response->setStatusCode(drogon::k401Unauthorized);
        applyCors(request, response);
        callback(response);
        return;
    }

    // 수정 대상 식재료 ID를 쿼리에서 읽는다.
    const auto ingredientId = extractIngredientIdParameter(request);
    if (!ingredientId.has_value())
    {
        auto response = drogon::HttpResponse::newHttpJsonResponse(
            makeErrorBody("Ingredient id is required."));
        response->setStatusCode(drogon::k400BadRequest);
        applyCors(request, response);
        callback(response);
        return;
    }

    // 본문이 유효한 JSON인지 확인한다.
    const auto json = request->getJsonObject();
    if (!json)
    {
        auto response =
            drogon::HttpResponse::newHttpJsonResponse(makeErrorBody("Invalid JSON body."));
        response->setStatusCode(drogon::k400BadRequest);
        applyCors(request, response);
        callback(response);
        return;
    }

    // JSON을 수정 DTO로 매핑한다.
    UpdateIngredientRequestDTO dto;
    dto.name = json->get("name", "").asString();
    dto.category = json->get("category", "").asString();
    dto.quantity = json->get("quantity", "").asString();
    dto.unit = json->get("unit", "").asString();
    dto.storage = json->get("storage", "").asString();
    dto.expiryDate = json->get("expiryDate", "").asString();

    // 수정 로직은 서비스 계층에 위임한다.
    const auto result =
        ingredientService_.updateIngredient(*memberId, *ingredientId, dto);

    // 공통 응답 필드를 구성한다.
    Json::Value body;
    body["ok"] = result.ok;
    body["message"] = result.message;

    // 수정된 식재료가 있으면 응답에 포함한다.
    if (result.ingredient.has_value())
    {
        body["ingredient"] = buildIngredientJson(*result.ingredient);
    }

    // JSON 응답 생성 후 상태코드를 반영한다.
    auto response = drogon::HttpResponse::newHttpJsonResponse(body);
    response->setStatusCode(static_cast<drogon::HttpStatusCode>(result.statusCode));

    // CORS 헤더 적용 후 응답한다.
    applyCors(request, response);
    callback(response);
}

// 식재료 삭제 요청을 처리한다.
void IngredientController::handleDeleteIngredient(
    const drogon::HttpRequestPtr &request,
    Callback &&callback)
{
    // 삭제 요청은 로그인 사용자 식별이 필요하다.
    const auto memberId = extractMemberIdHeader(request);
    if (!memberId.has_value())
    {
        auto response =
            drogon::HttpResponse::newHttpJsonResponse(makeErrorBody("Unauthorized."));
        response->setStatusCode(drogon::k401Unauthorized);
        applyCors(request, response);
        callback(response);
        return;
    }

    // 삭제 대상 식재료 ID를 쿼리에서 읽는다.
    const auto ingredientId = extractIngredientIdParameter(request);
    if (!ingredientId.has_value())
    {
        auto response = drogon::HttpResponse::newHttpJsonResponse(
            makeErrorBody("Ingredient id is required."));
        response->setStatusCode(drogon::k400BadRequest);
        applyCors(request, response);
        callback(response);
        return;
    }

    // 삭제 로직은 서비스 계층에 위임한다.
    const auto result =
        ingredientService_.deleteIngredient(*memberId, *ingredientId);

    // 공통 응답 필드를 구성한다.
    Json::Value body;
    body["ok"] = result.ok;
    body["message"] = result.message;

    // JSON 응답 생성 후 상태코드를 반영한다.
    auto response = drogon::HttpResponse::newHttpJsonResponse(body);
    response->setStatusCode(static_cast<drogon::HttpStatusCode>(result.statusCode));

    // CORS 헤더 적용 후 응답한다.
    applyCors(request, response);
    callback(response);
}

// 가공식품 검색 요청을 처리한다.
void IngredientController::handleSearchProcessedFoods(
    const drogon::HttpRequestPtr &request,
    Callback &&callback)
{
    // 검색 키워드를 쿼리 파라미터에서 읽는다.
    const auto keyword = request->getParameter("keyword");

    // 가공식품 검색은 서비스 계층의 비동기 콜백으로 처리한다.
    ingredientService_.searchProcessedFoods(
        keyword,
        [request, callback = std::move(callback)](
            ProcessedFoodSearchResultDTO result) mutable {
            // 공통 응답 필드를 구성한다.
            Json::Value body;
            body["ok"] = result.ok;
            body["message"] = result.message;

            // 검색 결과를 JSON 배열로 직렬화한다.
            Json::Value foods(Json::arrayValue);
            for (const auto &food : result.foods)
            {
                foods.append(buildProcessedFoodJson(food));
            }
            body["foods"] = foods;

            // JSON 응답 생성 후 상태코드를 반영한다.
            auto response = drogon::HttpResponse::newHttpJsonResponse(body);
            response->setStatusCode(
                static_cast<drogon::HttpStatusCode>(result.statusCode));

            // CORS 헤더 적용 후 응답한다.
            applyCors(request, response);
            callback(response);
        });
}

}  // namespace ingredient

