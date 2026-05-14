/*
 * 파일 개요: 공공 영양 OpenAPI 클라이언트 인터페이스 선언 파일이다.
 * 주요 역할: 카테고리 endpoint 열거형, 페이지 응답 구조, 단일 페이지 조회 계약을 정의한다.
 * 사용 위치: 검색 서비스 계층에서 외부 API 연동을 캡슐화하는 어댑터 타입으로 사용된다.
 */
#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

#include <drogon/HttpClient.h>
#include <json/value.h>

namespace ingredient
{

// 공공 영양 API 호출/파싱 전담 클라이언트.
class ProcessedFoodSearchService_Client
{
  public:
    // 카테고리별 endpoint 선택자.
    enum class CategoryEndpoint
    {
        Processed,
        Material,
        Food,
    };

    struct RowsPage
    {
        // 응답 row 목록.
        std::vector<Json::Value> rows;
        // totalCount가 응답에 있을 때만 채운다.
        std::optional<std::uint64_t> totalCount;
    };

    // 고정 호스트 HttpClient를 생성한다.
    drogon::HttpClientPtr makeClient() const;

    // 단일 페이지를 요청해 rows/totalCount를 파싱한다. 실패 시 nullopt.
    std::optional<RowsPage> requestRowsPage(
        const drogon::HttpClientPtr &client,
        const std::string &serviceKey,
        std::uint64_t pageNo,
        std::uint64_t numOfRows,
        const std::optional<std::string> &foodKeyword,
        const std::optional<std::string> &manufacturerName,
        const std::optional<std::string> &foodLevel4Name,
        std::string &errorMessage,
        CategoryEndpoint endpoint = CategoryEndpoint::Processed,
        const std::optional<std::string> &foodLevel3Name = std::nullopt) const;
};

}  // namespace ingredient

