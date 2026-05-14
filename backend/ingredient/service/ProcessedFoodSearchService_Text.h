/*
 * 파일 개요: 검색 텍스트/JSON 정규화 유틸 인터페이스 선언 파일이다.
 * 주요 역할: 문자열 보정, JSON 안전 변환, 검색 토큰/우선순위 계산 함수 계약을 제공한다.
 * 사용 위치: ingredient 검색 서비스 계층의 공통 헬퍼 모듈로 다수 구현 파일에서 include 된다.
 */
#pragma once

#include <cstddef>
#include <cstdint>
#include <initializer_list>
#include <optional>
#include <string>
#include <vector>

#include <json/value.h>

#include "ingredient/model/IngredientTypes.h"

namespace ingredient
{

// 문자열 정규화/JSON 안전 파싱/검색 우선순위 계산 유틸리티.
class ProcessedFoodSearchService_Text
{
  public:
    // 양끝 공백 제거.
    static std::string trim(std::string value);
    // ASCII 소문자 변환.
    static std::string toLowerAscii(std::string value);
    // 검색 비교용 정규화 키 생성(trim + lowercase + 공백 제거).
    static std::string normalizeForSearch(std::string value);
    // 이미 percent-encoding된 문자열인지 감지.
    static bool looksLikePercentEncoded(const std::string &value);

    // Json::Value를 optional<string>으로 안전 변환.
    static std::optional<std::string> optionalJsonString(const Json::Value &value);
    // 여러 key alias 중 첫 유효 문자열 값을 반환.
    static std::optional<std::string> firstStringFromKeys(
        const Json::Value &row,
        std::initializer_list<const char *> keys);
    // Json::Value를 optional<double>로 안전 변환.
    static std::optional<double> optionalJsonDouble(const Json::Value &value);
    // 여러 key alias 중 첫 유효 숫자값을 반환.
    static std::optional<double> firstDoubleFromKeys(
        const Json::Value &row,
        std::initializer_list<const char *> keys);
    // Json::Value를 optional<uint64_t>로 안전 변환.
    static std::optional<std::uint64_t> optionalJsonUInt64(const Json::Value &value);

    // importYn을 Y/N/null로 정규화.
    static std::optional<std::string> normalizeImportYnForApi(
        const std::optional<std::string> &value);
    // 날짜 문자열(YYYYMMDD)을 YYYY-MM-DD로 보정.
    static std::optional<std::string> normalizeDataBaseDate(
        const std::optional<std::string> &value);

    // 정규화된 token 포함 여부 검사.
    static bool containsNormalizedToken(const std::string &text,
                                        const std::string &token);
    // UTF-8 코드포인트 기준 prefix token 목록 생성.
    static std::vector<std::string> buildUtf8PrefixTokens(
        const std::string &keyword,
        std::size_t minCodePoints,
        std::size_t maxTokenCount);
    // exact/prefix/partial 우선순위 계산.
    static int searchPriority(const ProcessedFoodSearchItemDTO &food,
                              const std::string &normalizedKeywordToken);
};

}  // namespace ingredient


