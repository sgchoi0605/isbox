/*
 * 파일 개요: 공공 영양 OpenAPI 호출 및 응답 파싱 클라이언트 구현 파일이다.
 * 주요 역할: endpoint별 요청 경로 생성, 재시도 포함 HTTP 호출, 응답 스키마 파싱, API 오류 해석을 수행한다.
 * 사용 위치: ProcessedFoodSearchService/NutritionFoodsSearchService가 외부 데이터 수집 시 공통 호출 계층으로 사용한다.
 */
#include "ingredient/service/ProcessedFoodSearchService_Client.h"

#include <chrono>
#include <thread>

#include <drogon/drogon.h>
#include <drogon/utils/Utilities.h>

#include "ingredient/service/ProcessedFoodSearchService_Text.h"

namespace
{

using ingredient::ProcessedFoodSearchService_Text;
using CategoryEndpoint = ingredient::ProcessedFoodSearchService_Client::CategoryEndpoint;

// response.header의 resultCode/resultMsg를 읽어 비즈니스 오류를 해석한다.
std::optional<std::string> extractPublicApiErrorMessage(const Json::Value &body)
{
    if (!body.isMember("response") || !body["response"].isObject())
    {
        return std::nullopt;
    }

    const auto &responseRoot = body["response"];
    if (!responseRoot.isMember("header") || !responseRoot["header"].isObject())
    {
        return std::nullopt;
    }

    const auto &header = responseRoot["header"];
    const auto resultCode = ProcessedFoodSearchService_Text::firstStringFromKeys(
        header, {"resultCode", "RESULT_CODE"});
    const auto resultMsg = ProcessedFoodSearchService_Text::firstStringFromKeys(
        header, {"resultMsg", "RESULT_MSG"});
    if (!resultCode.has_value() || *resultCode == "00")
    {
        return std::nullopt;
    }

    return "Nutrition public API error (" + *resultCode +
           "): " + resultMsg.value_or("Unknown error.");
}

// totalCount 필드가 있을 때만 안전하게 정수로 파싱한다.
std::optional<std::uint64_t> extractTotalCount(const Json::Value &body)
{
    if (!body.isMember("response") || !body["response"].isObject())
    {
        return std::nullopt;
    }

    const auto &responseRoot = body["response"];
    if (!responseRoot.isMember("body") || !responseRoot["body"].isObject())
    {
        return std::nullopt;
    }

    const auto &bodyNode = responseRoot["body"];
    if (!bodyNode.isMember("totalCount"))
    {
        return std::nullopt;
    }

    return ProcessedFoodSearchService_Text::optionalJsonUInt64(bodyNode["totalCount"]);
}

// data/items/item 등 응답 스키마 변형을 모두 흡수해 row 배열을 추출한다.
std::vector<Json::Value> extractRows(const Json::Value &root)
{
    std::vector<Json::Value> out;

    if (root.isMember("data") && root["data"].isArray())
    {
        for (const auto &item : root["data"])
        {
            out.push_back(item);
        }
        return out;
    }

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
                        for (const auto &item : itemField)
                        {
                            out.push_back(item);
                        }
                        return out;
                    }

                    if (itemField.isObject())
                    {
                        out.push_back(itemField);
                        return out;
                    }
                }
            }
        }
    }

    return out;
}

// 값이 있을 때만 URL 인코딩해 query parameter를 추가한다.
void appendEncodedQueryParam(std::string &path,
                             const char *key,
                             const std::optional<std::string> &value)
{
    if (!value.has_value())
    {
        return;
    }

    const auto trimmed = ProcessedFoodSearchService_Text::trim(*value);
    if (trimmed.empty())
    {
        return;
    }

    const auto encoded =
        ProcessedFoodSearchService_Text::looksLikePercentEncoded(trimmed)
            ? trimmed
            : drogon::utils::urlEncodeComponent(trimmed);
    path += "&";
    path += key;
    path += "=";
    path += encoded;
}

// 카테고리별 endpoint와 선택 파라미터를 합쳐 최종 API 경로를 구성한다.
std::string buildPublicNutritionApiPath(
    const std::string &serviceKey,
    std::uint64_t pageNo,
    std::uint64_t numOfRows,
    const std::optional<std::string> &foodKeyword,
    const std::optional<std::string> &manufacturerName,
    const std::optional<std::string> &foodLevel4Name,
    const std::optional<std::string> &foodLevel3Name,
    CategoryEndpoint endpoint)
{
    const char *openApiPath = "/openapi/tn_pubr_public_nutri_process_info_api";
    if (endpoint == CategoryEndpoint::Material)
    {
        openApiPath = "/openapi/tn_pubr_public_nutri_material_info_api";
    }
    else if (endpoint == CategoryEndpoint::Food)
    {
        openApiPath = "/openapi/tn_pubr_public_nutri_food_info_api";
    }

    std::string path =
        std::string(openApiPath) + "?serviceKey=" +
        drogon::utils::urlEncodeComponent(serviceKey) + "&type=json&pageNo=" +
        std::to_string(pageNo) + "&numOfRows=" + std::to_string(numOfRows);

    appendEncodedQueryParam(path, "foodNm", foodKeyword);
    appendEncodedQueryParam(path, "mfrNm", manufacturerName);
    appendEncodedQueryParam(path, "foodLv4Nm", foodLevel4Name);
    appendEncodedQueryParam(path, "foodLv3Nm", foodLevel3Name);
    return path;
}

}  // namespace

namespace ingredient
{

// 공공 API 호스트 전용 HttpClient를 생성한다.
drogon::HttpClientPtr ProcessedFoodSearchService_Client::makeClient() const
{
    return drogon::HttpClient::newHttpClient("https://27.101.215.193",
                                             nullptr,
                                             false,
                                             false);
}

std::optional<ProcessedFoodSearchService_Client::RowsPage>
ProcessedFoodSearchService_Client::requestRowsPage(
    const drogon::HttpClientPtr &client,
    const std::string &serviceKey,
    std::uint64_t pageNo,
    std::uint64_t numOfRows,
    const std::optional<std::string> &foodKeyword,
    const std::optional<std::string> &manufacturerName,
    const std::optional<std::string> &foodLevel4Name,
    std::string &errorMessage,
    CategoryEndpoint endpoint,
    const std::optional<std::string> &foodLevel3Name) const
{
    // 요청 경로를 먼저 확정해 재시도 시 동일 요청을 보장한다.
    const auto path = buildPublicNutritionApiPath(serviceKey,
                                                  pageNo,
                                                  numOfRows,
                                                  foodKeyword,
                                                  manufacturerName,
                                                  foodLevel4Name,
                                                  foodLevel3Name,
                                                  endpoint);
    auto buildRequest = [&path]() {
        auto request = drogon::HttpRequest::newHttpRequest();
        request->setMethod(drogon::Get);
        request->setPathEncode(false);
        request->setPath(path);
        request->addHeader("Host", "api.data.go.kr");
        return request;
    };

    drogon::ReqResult reqResult = drogon::ReqResult::NetworkFailure;
    drogon::HttpResponsePtr response;
    constexpr int kMaxTransportAttempts = 2;
    // 일시적 네트워크 오류를 줄이기 위해 짧게 재시도한다.
    for (int attempt = 1; attempt <= kMaxTransportAttempts; ++attempt)
    {
        auto request = buildRequest();
        std::tie(reqResult, response) = client->sendRequest(request, 20.0);
        if (reqResult == drogon::ReqResult::Ok && response)
        {
            break;
        }

        if (attempt < kMaxTransportAttempts)
        {
            std::this_thread::sleep_for(std::chrono::milliseconds(120));
        }
    }
    // 전송 자체가 실패하면 네트워크 오류로 처리한다.
    if (reqResult != drogon::ReqResult::Ok || !response)
    {
        errorMessage = "Failed to call nutrition public API. (ReqResult: " +
                       drogon::to_string(reqResult) + ")";
        return std::nullopt;
    }

    // HTTP 상태코드가 200이 아니면 실패로 처리한다.
    if (response->statusCode() != drogon::k200OK)
    {
        errorMessage = "Nutrition public API returned error.";
        return std::nullopt;
    }

    // JSON 본문이 없으면 스키마 오류로 본다.
    const auto body = response->getJsonObject();
    if (!body)
    {
        errorMessage = "Invalid response from nutrition public API.";
        return std::nullopt;
    }

    // API 비즈니스 오류코드를 별도로 해석한다.
    if (const auto apiError = extractPublicApiErrorMessage(*body); apiError.has_value())
    {
        // resultCode=03은 '데이터 없음' 케이스이므로 빈 성공 페이지로 취급한다.
        if (apiError->find("Nutrition public API error (03)") != std::string::npos)
        {
            RowsPage emptyPage;
            emptyPage.totalCount = 0U;
            return emptyPage;
        }

        errorMessage = *apiError;
        return std::nullopt;
    }

    RowsPage page;
    page.rows = extractRows(*body);
    page.totalCount = extractTotalCount(*body);
    return page;
}

}  // namespace ingredient


