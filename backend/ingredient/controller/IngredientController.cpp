/*
 * 파일 개요: ingredient API 엔드포인트의 요청 파싱과 응답 생성을 구현한 컨트롤러 파일이다.
 * 핵심 책임: HTTP 요청 -> DTO 변환 -> 서비스 호출 -> HTTP JSON 응답 직렬화 흐름을 담당한다.
 * 분리 원칙: 비즈니스 판단(검증/권한/DB)은 service 계층에 위임하고, 여기서는 프로토콜 처리에 집중한다.
 */

#include "ingredient/controller/IngredientController.h"  // 컨트롤러 선언, DTO 타입, 서비스 멤버 정의를 포함한다.

#include <algorithm>  // std::remove_if: 문자열 숫자 파싱 전 문자 제거에 사용한다.
#include <cctype>     // std::isspace: 공백 문자 판별에 사용한다.
#include <exception>  // std::exception: 숫자 변환 실패 예외를 안전하게 흡수한다.
#include <string>     // std::string: 헤더/쿼리/JSON 문자열 처리에 사용한다.
#include <utility>    // std::move: 콜백 소유권 이동에 사용한다.

namespace
{

// 공통 에러 응답 JSON 본문을 만든다.
Json::Value makeErrorBody(const std::string &message)
{
    Json::Value body;              // 응답 루트 JSON 객체.
    body["ok"] = false;            // 실패 응답임을 명시.
    body["message"] = message;     // 사용자/클라이언트 노출용 메시지.
    return body;                   // 완성된 에러 본문 반환.
}

// JSON 키가 "유효한 문자열"일 때만 optional<string>으로 꺼낸다.
std::optional<std::string> readOptionalString(const Json::Value &json,
                                              const char *key)
{
    if (!json.isMember(key))       // 키 자체가 없으면 값 없음 처리.
    {
        return std::nullopt;
    }

    const auto &value = json[key]; // 키 값을 참조로 가져온다.
    if (value.isNull() || !value.isString())  // null이거나 문자열 타입이 아니면 무시.
    {
        return std::nullopt;
    }

    const auto text = value.asString();  // JSON 문자열을 C++ 문자열로 변환.
    if (text.empty())                    // 빈 문자열은 의미 없는 값으로 간주.
    {
        return std::nullopt;
    }
    return text;                         // 유효 문자열 반환.
}

// JSON 키를 optional<double>로 읽는다(숫자 타입/숫자 문자열 모두 허용).
std::optional<double> readOptionalDouble(const Json::Value &json,
                                         const char *key)
{
    if (!json.isMember(key))             // 키가 없으면 값 없음.
    {
        return std::nullopt;
    }

    const auto &value = json[key];       // 키 값을 참조로 가져온다.
    if (value.isNull())                  // 명시적 null이면 값 없음.
    {
        return std::nullopt;
    }

    try
    {
        if (value.isNumeric())           // JSON 숫자 타입이면 바로 double 변환.
        {
            return value.asDouble();
        }

        if (value.isString())            // 문자열 숫자도 허용한다.
        {
            auto text = value.asString();  // 원본 문자열 획득.

            // 공백과 쉼표를 제거해 "1,234.5" 같은 형태도 파싱 가능하게 만든다.
            text.erase(
                std::remove_if(text.begin(),
                               text.end(),
                               [](unsigned char ch)
                               {
                                   return std::isspace(ch) != 0 || ch == ',';
                               }),
                text.end());

            if (text.empty())            // 제거 후 빈 문자열이면 무효.
            {
                return std::nullopt;
            }
            return std::stod(text);      // 문자열을 double로 변환.
        }
    }
    catch (const std::exception &)       // stoull/stod 변환 실패 등 모든 예외 흡수.
    {
        return std::nullopt;
    }

    return std::nullopt;                 // 허용 타입이 아니면 값 없음 처리.
}

}  // namespace

namespace ingredient
{

void IngredientController::registerHandlers()
{
    auto &app = drogon::app();  // Drogon 전역 앱 인스턴스를 참조.

    // GET /api/ingredients -> 식재료 목록 조회 핸들러 연결.
    app.registerHandler(
        "/api/ingredients",
        [this](const drogon::HttpRequestPtr &request, Callback &&callback)
        {
            handleListIngredients(request, std::move(callback));  // 실제 처리 함수로 위임.
        },
        {drogon::Get});

    // POST /api/ingredients -> 식재료 생성 핸들러 연결.
    app.registerHandler(
        "/api/ingredients",
        [this](const drogon::HttpRequestPtr &request, Callback &&callback)
        {
            handleCreateIngredient(request, std::move(callback));  // 실제 처리 함수로 위임.
        },
        {drogon::Post});

    // PUT /api/ingredients -> 식재료 수정 핸들러 연결.
    app.registerHandler(
        "/api/ingredients",
        [this](const drogon::HttpRequestPtr &request, Callback &&callback)
        {
            handleUpdateIngredient(request, std::move(callback));  // 실제 처리 함수로 위임.
        },
        {drogon::Put});

    // DELETE /api/ingredients -> 식재료 삭제 핸들러 연결.
    app.registerHandler(
        "/api/ingredients",
        [this](const drogon::HttpRequestPtr &request, Callback &&callback)
        {
            handleDeleteIngredient(request, std::move(callback));  // 실제 처리 함수로 위임.
        },
        {drogon::Delete});

    // GET /api/nutrition/processed-foods -> 가공식품 검색 핸들러 연결.
    app.registerHandler(
        "/api/nutrition/processed-foods",
        [this](const drogon::HttpRequestPtr &request, Callback &&callback)
        {
            handleSearchProcessedFoods(request, std::move(callback));  // 실제 처리 함수로 위임.
        },
        {drogon::Get});

    // GET /api/nutrition/foods -> 통합 식품 검색 핸들러 연결.
    app.registerHandler(
        "/api/nutrition/foods",
        [this](const drogon::HttpRequestPtr &request, Callback &&callback)
        {
            handleSearchFoods(request, std::move(callback));  // 실제 처리 함수로 위임.
        },
        {drogon::Get});
}

Json::Value IngredientController::buildIngredientJson(
    const IngredientDTO &ingredient)
{
    Json::Value item;  // 식재료 단건을 담을 JSON 객체.

    // ID는 문자열/숫자 둘 다 내려 프런트 구현체 간 호환성을 맞춘다.
    item["id"] = std::to_string(ingredient.ingredientId);                   // 문자열 ID.
    item["ingredientId"] = static_cast<Json::UInt64>(ingredient.ingredientId);  // 숫자 ID.
    item["memberId"] = static_cast<Json::UInt64>(ingredient.memberId);      // 소유 회원 ID.

    if (ingredient.publicFoodCode.has_value())   // 공공 식품 코드가 있을 때만 포함.
    {
        item["publicFoodCode"] = *ingredient.publicFoodCode;
    }

    item["name"] = ingredient.name;              // 식재료명.
    item["category"] = ingredient.category;      // 식재료 카테고리.
    item["quantity"] = ingredient.quantity;      // 수량(문자열 표현 허용).
    item["unit"] = ingredient.unit;              // 수량 단위.
    item["storage"] = ingredient.storage;        // 보관 방법.
    item["expiryDate"] = ingredient.expiryDate;  // 유통/소비 기한.

    if (ingredient.nutritionBasisAmount.has_value())  // 영양 성분 기준량.
    {
        item["nutritionBasisAmount"] = *ingredient.nutritionBasisAmount;
    }
    if (ingredient.energyKcal.has_value())       // 열량(kcal).
    {
        item["energyKcal"] = *ingredient.energyKcal;
    }
    if (ingredient.proteinG.has_value())         // 단백질(g).
    {
        item["proteinG"] = *ingredient.proteinG;
    }
    if (ingredient.fatG.has_value())             // 지방(g).
    {
        item["fatG"] = *ingredient.fatG;
    }
    if (ingredient.carbohydrateG.has_value())    // 탄수화물(g).
    {
        item["carbohydrateG"] = *ingredient.carbohydrateG;
    }
    if (ingredient.sugarG.has_value())           // 당류(g).
    {
        item["sugarG"] = *ingredient.sugarG;
    }
    if (ingredient.sodiumMg.has_value())         // 나트륨(mg).
    {
        item["sodiumMg"] = *ingredient.sodiumMg;
    }

    if (ingredient.sourceName.has_value())       // 원천 데이터 출처명.
    {
        item["sourceName"] = *ingredient.sourceName;
    }
    if (ingredient.manufacturerName.has_value()) // 제조사명.
    {
        item["manufacturerName"] = *ingredient.manufacturerName;
    }
    if (ingredient.importYn.has_value())         // 수입 여부(Y/N).
    {
        item["importYn"] = *ingredient.importYn;
    }
    if (ingredient.originCountryName.has_value())  // 원산지 국가명.
    {
        item["originCountryName"] = *ingredient.originCountryName;
    }
    if (ingredient.dataBaseDate.has_value())     // 원본 데이터 기준일.
    {
        item["dataBaseDate"] = *ingredient.dataBaseDate;
    }

    item["createdAt"] = ingredient.createdAt;    // 생성 시각.
    item["updatedAt"] = ingredient.updatedAt;    // 수정 시각.
    return item;                                 // 직렬화된 식재료 JSON 반환.
}

Json::Value IngredientController::buildProcessedFoodJson(
    const ProcessedFoodSearchItemDTO &food)
{
    Json::Value item;  // 검색 결과 단건 JSON 객체.

    item["foodCode"] = food.foodCode;            // 식품 코드.
    item["foodName"] = food.foodName;            // 식품 이름.

    if (food.foodGroupName.has_value())          // 식품군명(있을 때만 포함).
    {
        item["foodGroupName"] = *food.foodGroupName;
    }
    if (!food.sourceType.empty())                // 소스 타입 키(processed/material/food).
    {
        item["sourceType"] = food.sourceType;
    }
    if (!food.sourceTypeLabel.empty())           // 소스 타입 표시명.
    {
        item["sourceTypeLabel"] = food.sourceTypeLabel;
    }

    if (food.nutritionBasisAmount.has_value())   // 영양 성분 기준량.
    {
        item["nutritionBasisAmount"] = *food.nutritionBasisAmount;
    }
    if (food.energyKcal.has_value())             // 열량(kcal).
    {
        item["energyKcal"] = *food.energyKcal;
    }
    if (food.proteinG.has_value())               // 단백질(g).
    {
        item["proteinG"] = *food.proteinG;
    }
    if (food.fatG.has_value())                   // 지방(g).
    {
        item["fatG"] = *food.fatG;
    }
    if (food.carbohydrateG.has_value())          // 탄수화물(g).
    {
        item["carbohydrateG"] = *food.carbohydrateG;
    }
    if (food.sugarG.has_value())                 // 당류(g).
    {
        item["sugarG"] = *food.sugarG;
    }
    if (food.sodiumMg.has_value())               // 나트륨(mg).
    {
        item["sodiumMg"] = *food.sodiumMg;
    }

    if (food.sourceName.has_value())             // 데이터 출처명.
    {
        item["sourceName"] = *food.sourceName;
    }
    if (food.manufacturerName.has_value())       // 제조사명.
    {
        item["manufacturerName"] = *food.manufacturerName;
    }
    if (food.importYn.has_value())               // 수입 여부(Y/N).
    {
        item["importYn"] = *food.importYn;
    }
    if (food.originCountryName.has_value())      // 원산지 국가명.
    {
        item["originCountryName"] = *food.originCountryName;
    }
    if (food.dataBaseDate.has_value())           // 원본 데이터 기준일.
    {
        item["dataBaseDate"] = *food.dataBaseDate;
    }

    return item;                                 // 직렬화된 검색 결과 반환.
}

void IngredientController::applyCors(const drogon::HttpRequestPtr &request,
                                     const drogon::HttpResponsePtr &response)
{
    std::string origin = request->getHeader("Origin");  // 표준 형태 Origin 헤더를 먼저 읽는다.
    if (origin.empty())                                 // 비표준/소문자 전송을 대비한다.
    {
        origin = request->getHeader("origin");
    }

    if (!origin.empty())                                // Origin 값이 있을 때만 동적으로 반영.
    {
        response->addHeader("Access-Control-Allow-Origin",
                            origin);                    // 해당 Origin 요청을 허용.
        response->addHeader("Vary",
                            "Origin");                  // 캐시 분기를 위해 Vary 헤더를 설정.
    }

    response->addHeader("Access-Control-Allow-Credentials",
                        "true");                        // 쿠키/인증정보 포함 요청 허용.
    response->addHeader("Access-Control-Allow-Headers",
                        "Content-Type, X-Member-Id");  // 요청 허용 헤더 목록.
    response->addHeader("Access-Control-Allow-Methods",
                        "GET, POST, PUT, DELETE, OPTIONS");  // 허용 메서드 목록.
}

std::optional<std::uint64_t> IngredientController::extractMemberIdHeader(
    const drogon::HttpRequestPtr &request)
{
    auto memberIdValue = request->getHeader("X-Member-Id");  // 표준 헤더명 조회.
    if (memberIdValue.empty())                               // 소문자 헤더명도 허용.
    {
        memberIdValue = request->getHeader("x-member-id");
    }

    if (memberIdValue.empty())                               // 헤더가 없으면 인증 실패로 판단.
    {
        return std::nullopt;
    }

    try
    {
        const auto parsed = std::stoull(memberIdValue);     // 문자열을 uint64로 변환.
        if (parsed == 0)                                     // 0은 유효 사용자 ID로 보지 않는다.
        {
            return std::nullopt;
        }
        return parsed;                                       // 정상 파싱된 회원 ID 반환.
    }
    catch (const std::exception &)                          // 숫자 변환 실패 시.
    {
        return std::nullopt;
    }
}

std::optional<std::uint64_t> IngredientController::extractIngredientIdParameter(
    const drogon::HttpRequestPtr &request)
{
    const auto value = request->getParameter("ingredientId");  // 쿼리 파라미터 추출.
    if (value.empty())                                         // 파라미터가 없으면 실패.
    {
        return std::nullopt;
    }

    try
    {
        const auto parsed = std::stoull(value);                // 문자열 -> uint64 변환.
        if (parsed == 0)                                       // 0은 무효 ID로 간주.
        {
            return std::nullopt;
        }
        return parsed;                                         // 정상 파싱된 식재료 ID 반환.
    }
    catch (const std::exception &)                            // 파싱 실패 시.
    {
        return std::nullopt;
    }
}

void IngredientController::handleListIngredients(
    const drogon::HttpRequestPtr &request,
    Callback &&callback)
{
    const auto memberId = extractMemberIdHeader(request);      // 인증 헤더에서 회원 ID 추출.
    if (!memberId.has_value())                                 // 인증 헤더가 없거나 잘못된 경우.
    {
        auto response =
            drogon::HttpResponse::newHttpJsonResponse(makeErrorBody("Unauthorized."));
        response->setStatusCode(drogon::k401Unauthorized);     // 401 상태 코드 설정.
        applyCors(request, response);                          // CORS 헤더 추가.
        callback(response);                                    // 즉시 응답 반환.
        return;
    }

    const auto result = ingredientService_.getIngredients(*memberId);  // 서비스에서 회원 목록 조회.

    Json::Value body;                                           // 성공/실패 공통 응답 루트.
    body["ok"] = result.ok;                                     // 처리 성공 여부.
    body["message"] = result.message;                           // 처리 결과 메시지.

    Json::Value items(Json::arrayValue);                        // 식재료 배열 컨테이너.
    for (const auto &ingredient : result.ingredients)           // 서비스 결과 목록 반복.
    {
        items.append(buildIngredientJson(ingredient));          // DTO -> JSON 변환 후 배열에 추가.
    }
    body["ingredients"] = items;                                // 배열을 본문에 주입.

    auto response = drogon::HttpResponse::newHttpJsonResponse(body);  // JSON 응답 객체 생성.
    response->setStatusCode(static_cast<drogon::HttpStatusCode>(
        result.statusCode));                                    // 서비스가 지정한 HTTP 상태 적용.

    applyCors(request, response);                               // CORS 헤더 추가.
    callback(response);                                         // 응답 전송.
}

void IngredientController::handleCreateIngredient(
    const drogon::HttpRequestPtr &request,
    Callback &&callback)
{
    const auto memberId = extractMemberIdHeader(request);      // 인증 헤더에서 회원 ID 추출.
    if (!memberId.has_value())                                 // 인증 실패 시.
    {
        auto response =
            drogon::HttpResponse::newHttpJsonResponse(makeErrorBody("Unauthorized."));
        response->setStatusCode(drogon::k401Unauthorized);     // 401 반환.
        applyCors(request, response);                          // CORS 헤더 추가.
        callback(response);                                    // 즉시 응답 반환.
        return;
    }

    const auto json = request->getJsonObject();                // 요청 본문을 JSON으로 파싱.
    if (!json)                                                 // 파싱 실패 시.
    {
        auto response =
            drogon::HttpResponse::newHttpJsonResponse(makeErrorBody("Invalid JSON body."));
        response->setStatusCode(drogon::k400BadRequest);       // 잘못된 요청 본문 400 반환.
        applyCors(request, response);                          // CORS 헤더 추가.
        callback(response);                                    // 즉시 응답 반환.
        return;
    }

    CreateIngredientRequestDTO dto;                            // 생성 요청 DTO 준비.
    dto.name = json->get("name", "").asString();               // 이름 필드 매핑.
    dto.category = json->get("category", "").asString();       // 카테고리 필드 매핑.
    dto.quantity = json->get("quantity", "").asString();       // 수량 필드 매핑.
    dto.unit = json->get("unit", "").asString();               // 단위 필드 매핑.
    dto.storage = json->get("storage", "").asString();         // 보관 방식 필드 매핑.
    dto.expiryDate = json->get("expiryDate", "").asString();   // 만료일 필드 매핑.

    dto.publicFoodCode = readOptionalString(*json, "publicFoodCode");              // 공공 식품코드.
    dto.nutritionBasisAmount = readOptionalString(*json, "nutritionBasisAmount");  // 영양 기준량.
    dto.energyKcal = readOptionalDouble(*json, "energyKcal");                       // 열량.
    dto.proteinG = readOptionalDouble(*json, "proteinG");                           // 단백질.
    dto.fatG = readOptionalDouble(*json, "fatG");                                   // 지방.
    dto.carbohydrateG = readOptionalDouble(*json, "carbohydrateG");                 // 탄수화물.
    dto.sugarG = readOptionalDouble(*json, "sugarG");                               // 당류.
    dto.sodiumMg = readOptionalDouble(*json, "sodiumMg");                           // 나트륨.
    dto.sourceName = readOptionalString(*json, "sourceName");                       // 데이터 출처명.
    dto.manufacturerName = readOptionalString(*json, "manufacturerName");           // 제조사명.
    dto.importYn = readOptionalString(*json, "importYn");                           // 수입 여부.
    dto.originCountryName = readOptionalString(*json, "originCountryName");         // 원산지 국가명.
    dto.dataBaseDate = readOptionalString(*json, "dataBaseDate");                   // 원본 데이터 기준일.

    const auto result =
        ingredientService_.createIngredient(*memberId, dto);   // 서비스에 생성 처리 위임.

    Json::Value body;                                          // 응답 본문 구성.
    body["ok"] = result.ok;                                    // 성공 여부.
    body["message"] = result.message;                          // 처리 메시지.

    if (result.ingredient.has_value())                         // 생성 결과 엔티티가 있으면.
    {
        body["ingredient"] = buildIngredientJson(*result.ingredient);  // 단건 엔티티 직렬화.
    }

    auto response = drogon::HttpResponse::newHttpJsonResponse(body);  // JSON 응답 객체 생성.
    response->setStatusCode(static_cast<drogon::HttpStatusCode>(
        result.statusCode));                                   // 서비스 상태 코드 반영.

    applyCors(request, response);                              // CORS 헤더 추가.
    callback(response);                                        // 응답 전송.
}

void IngredientController::handleUpdateIngredient(
    const drogon::HttpRequestPtr &request,
    Callback &&callback)
{
    const auto memberId = extractMemberIdHeader(request);      // 인증 헤더에서 회원 ID 추출.
    if (!memberId.has_value())                                 // 인증 실패 시.
    {
        auto response =
            drogon::HttpResponse::newHttpJsonResponse(makeErrorBody("Unauthorized."));
        response->setStatusCode(drogon::k401Unauthorized);     // 401 반환.
        applyCors(request, response);                          // CORS 헤더 추가.
        callback(response);                                    // 즉시 응답 반환.
        return;
    }

    const auto ingredientId = extractIngredientIdParameter(request);  // 대상 식재료 ID 추출.
    if (!ingredientId.has_value())                                     // ID 누락/파싱 실패 시.
    {
        auto response = drogon::HttpResponse::newHttpJsonResponse(
            makeErrorBody("Ingredient id is required."));
        response->setStatusCode(drogon::k400BadRequest);       // 잘못된 요청 400 반환.
        applyCors(request, response);                          // CORS 헤더 추가.
        callback(response);                                    // 즉시 응답 반환.
        return;
    }

    const auto json = request->getJsonObject();                // 요청 본문 JSON 파싱.
    if (!json)                                                 // 파싱 실패 시.
    {
        auto response =
            drogon::HttpResponse::newHttpJsonResponse(makeErrorBody("Invalid JSON body."));
        response->setStatusCode(drogon::k400BadRequest);       // 400 반환.
        applyCors(request, response);                          // CORS 헤더 추가.
        callback(response);                                    // 즉시 응답 반환.
        return;
    }

    UpdateIngredientRequestDTO dto;                            // 수정 요청 DTO 준비.
    dto.name = json->get("name", "").asString();               // 이름 필드 매핑.
    dto.category = json->get("category", "").asString();       // 카테고리 필드 매핑.
    dto.quantity = json->get("quantity", "").asString();       // 수량 필드 매핑.
    dto.unit = json->get("unit", "").asString();               // 단위 필드 매핑.
    dto.storage = json->get("storage", "").asString();         // 보관 방식 필드 매핑.
    dto.expiryDate = json->get("expiryDate", "").asString();   // 만료일 필드 매핑.

    const auto result =
        ingredientService_.updateIngredient(*memberId,
                                            *ingredientId,
                                            dto);               // 서비스에 수정 처리 위임.

    Json::Value body;                                          // 응답 본문 구성.
    body["ok"] = result.ok;                                    // 성공 여부.
    body["message"] = result.message;                          // 처리 메시지.

    if (result.ingredient.has_value())                         // 수정 후 엔티티가 반환되면.
    {
        body["ingredient"] = buildIngredientJson(*result.ingredient);  // 단건 엔티티 직렬화.
    }

    auto response = drogon::HttpResponse::newHttpJsonResponse(body);  // JSON 응답 객체 생성.
    response->setStatusCode(static_cast<drogon::HttpStatusCode>(
        result.statusCode));                                   // 서비스 상태 코드 반영.

    applyCors(request, response);                              // CORS 헤더 추가.
    callback(response);                                        // 응답 전송.
}

void IngredientController::handleDeleteIngredient(
    const drogon::HttpRequestPtr &request,
    Callback &&callback)
{
    const auto memberId = extractMemberIdHeader(request);      // 인증 헤더에서 회원 ID 추출.
    if (!memberId.has_value())                                 // 인증 실패 시.
    {
        auto response =
            drogon::HttpResponse::newHttpJsonResponse(makeErrorBody("Unauthorized."));
        response->setStatusCode(drogon::k401Unauthorized);     // 401 반환.
        applyCors(request, response);                          // CORS 헤더 추가.
        callback(response);                                    // 즉시 응답 반환.
        return;
    }

    const auto ingredientId = extractIngredientIdParameter(request);  // 대상 식재료 ID 추출.
    if (!ingredientId.has_value())                                     // ID 누락/파싱 실패 시.
    {
        auto response = drogon::HttpResponse::newHttpJsonResponse(
            makeErrorBody("Ingredient id is required."));
        response->setStatusCode(drogon::k400BadRequest);       // 잘못된 요청 400 반환.
        applyCors(request, response);                          // CORS 헤더 추가.
        callback(response);                                    // 즉시 응답 반환.
        return;
    }

    const auto result =
        ingredientService_.deleteIngredient(*memberId,
                                            *ingredientId);    // 서비스에 삭제 처리 위임.

    Json::Value body;                                          // 응답 본문 구성.
    body["ok"] = result.ok;                                    // 성공 여부.
    body["message"] = result.message;                          // 처리 메시지.

    auto response = drogon::HttpResponse::newHttpJsonResponse(body);  // JSON 응답 객체 생성.
    response->setStatusCode(static_cast<drogon::HttpStatusCode>(
        result.statusCode));                                   // 서비스 상태 코드 반영.

    applyCors(request, response);                              // CORS 헤더 추가.
    callback(response);                                        // 응답 전송.
}

void IngredientController::handleSearchProcessedFoods(
    const drogon::HttpRequestPtr &request,
    Callback &&callback)
{
    const auto keyword = request->getParameter("keyword");     // keyword 쿼리 파라미터 추출.

    processedFoodSearchService_.searchProcessedFoods(
        keyword,                                                // 서비스에 검색 키워드 전달.
        [request, callback = std::move(callback)](
            ProcessedFoodSearchResultDTO result) mutable
        {
            Json::Value body;                                  // 응답 루트 JSON.
            body["ok"] = result.ok;                            // 성공 여부.
            body["message"] = result.message;                  // 처리 메시지.

            Json::Value foods(Json::arrayValue);               // 검색 결과 배열.
            for (const auto &food : result.foods)              // 서비스가 반환한 결과 반복.
            {
                foods.append(buildProcessedFoodJson(food));    // DTO -> JSON 변환 후 추가.
            }
            body["foods"] = foods;                             // 결과 배열을 본문에 포함.

            auto response =
                drogon::HttpResponse::newHttpJsonResponse(body);  // JSON 응답 생성.
            response->setStatusCode(static_cast<drogon::HttpStatusCode>(
                result.statusCode));                           // 서비스 상태 코드 반영.

            applyCors(request, response);                      // CORS 헤더 추가.
            callback(response);                                // 비동기 콜백으로 응답 반환.
        });
}

void IngredientController::handleSearchFoods(
    const drogon::HttpRequestPtr &request,
    Callback &&callback)
{
    const auto keyword = request->getParameter("keyword");     // keyword 쿼리 파라미터 추출.
    const auto sourceTypeRaw = request->getParameter("sourceType");
    const auto pageRaw = request->getParameter("page");
    const auto pageSizeRaw = request->getParameter("pageSize");

    auto parsePositiveUInt64 =
        [](const std::string &raw, std::uint64_t fallbackValue)
        -> std::optional<std::uint64_t> {
        auto text = raw;
        text.erase(text.begin(),
                   std::find_if(text.begin(),
                                text.end(),
                                [](unsigned char ch) { return std::isspace(ch) == 0; }));
        text.erase(std::find_if(text.rbegin(),
                                text.rend(),
                                [](unsigned char ch) { return std::isspace(ch) == 0; }).base(),
                   text.end());
        if (text.empty())
        {
            return fallbackValue;
        }

        try
        {
            const auto parsed = std::stoull(text);
            if (parsed == 0U)
            {
                return std::nullopt;
            }
            return static_cast<std::uint64_t>(parsed);
        }
        catch (const std::exception &)
        {
            return std::nullopt;
        }
    };

    std::optional<std::string> sourceType;
    auto trimmedSourceType = sourceTypeRaw;
    trimmedSourceType.erase(
        trimmedSourceType.begin(),
        std::find_if(trimmedSourceType.begin(),
                     trimmedSourceType.end(),
                     [](unsigned char ch) { return std::isspace(ch) == 0; }));
    trimmedSourceType.erase(
        std::find_if(trimmedSourceType.rbegin(),
                     trimmedSourceType.rend(),
                     [](unsigned char ch) { return std::isspace(ch) == 0; }).base(),
        trimmedSourceType.end());
    if (!trimmedSourceType.empty())
    {
        sourceType = trimmedSourceType;
    }

    const auto parsedPage = parsePositiveUInt64(pageRaw, 1U);
    const auto parsedPageSize = parsePositiveUInt64(pageSizeRaw, 5U);
    if (!parsedPage.has_value() || !parsedPageSize.has_value())
    {
        auto response = drogon::HttpResponse::newHttpJsonResponse(
            makeErrorBody("page/pageSize must be positive integers."));
        response->setStatusCode(drogon::k400BadRequest);
        applyCors(request, response);
        callback(response);
        return;
    }

    NutritionFoodsSearchService::SearchOptions options;
    options.sourceType = sourceType;
    options.page = *parsedPage;
    options.pageSize = *parsedPageSize;

    nutritionFoodsSearchService_.searchFoods(
        keyword,                                                // 서비스에 검색 키워드 전달.
        options,
        [request, callback = std::move(callback)](
            NutritionFoodsSearchResultDTO result) mutable
        {
            Json::Value body;                                  // 응답 루트 JSON.
            body["ok"] = result.ok;                            // 성공 여부.
            body["message"] = result.message;                  // 처리 메시지.

            Json::Value groups(Json::objectValue);             // 카테고리 그룹 객체.
            Json::Value processed(Json::arrayValue);           // processed 그룹 배열.
            Json::Value material(Json::arrayValue);            // material 그룹 배열.
            Json::Value food(Json::arrayValue);                // food 그룹 배열.

            for (const auto &item : result.groups.processed)   // processed 그룹 반복.
            {
                processed.append(buildProcessedFoodJson(item)); // DTO -> JSON 변환 후 추가.
            }
            for (const auto &item : result.groups.material)    // material 그룹 반복.
            {
                material.append(buildProcessedFoodJson(item));  // DTO -> JSON 변환 후 추가.
            }
            for (const auto &item : result.groups.food)        // food 그룹 반복.
            {
                food.append(buildProcessedFoodJson(item));      // DTO -> JSON 변환 후 추가.
            }

            groups["processed"] = processed;                   // processed 배열 장착.
            groups["material"] = material;                     // material 배열 장착.
            groups["food"] = food;                             // food 배열 장착.
            body["groups"] = groups;                           // 그룹 객체를 본문에 포함.

            Json::Value counts(Json::objectValue);             // 카테고리별 개수 객체.
            counts["processed"] =
                static_cast<Json::UInt64>(result.counts.processed);  // processed 개수.
            counts["material"] =
                static_cast<Json::UInt64>(result.counts.material);   // material 개수.
            counts["food"] =
                static_cast<Json::UInt64>(result.counts.food);       // food 개수.
            body["counts"] = counts;                           // 개수 객체 포함.

            Json::Value hasMore(Json::objectValue);
            hasMore["processed"] = result.hasMore.processed;
            hasMore["material"] = result.hasMore.material;
            hasMore["food"] = result.hasMore.food;
            body["hasMore"] = hasMore;

            Json::Value warnings(Json::arrayValue);            // 부분 실패 경고 배열.
            for (const auto &warning : result.warnings)        // 경고 목록 반복.
            {
                warnings.append(warning);                      // 경고 메시지 추가.
            }
            body["warnings"] = warnings;                       // 경고 배열 포함.

            Json::Value failedCategories(Json::arrayValue);    // 실패 카테고리 배열.
            for (const auto &failedCategory : result.failedCategories)  // 실패 카테고리 반복.
            {
                failedCategories.append(failedCategory);       // 실패 카테고리 키 추가.
            }
            body["failedCategories"] = failedCategories;       // 실패 카테고리 배열 포함.

            auto response =
                drogon::HttpResponse::newHttpJsonResponse(body);  // JSON 응답 생성.
            response->setStatusCode(static_cast<drogon::HttpStatusCode>(
                result.statusCode));                           // 서비스 상태 코드 반영.

            applyCors(request, response);                      // CORS 헤더 추가.
            callback(response);                                // 비동기 콜백으로 응답 반환.
        });
}

}  // namespace ingredient
