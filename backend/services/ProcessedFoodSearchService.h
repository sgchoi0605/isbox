#pragma once  // 이 헤더 파일이 여러 번 포함되어도 한 번만 처리되도록 한다.

#include <functional>  // std::function 타입(콜백 표현)을 사용하기 위해 포함한다.
#include <optional>    // std::optional 타입(값의 존재/부재 표현)을 사용하기 위해 포함한다.
#include <string>      // std::string 타입(문자열 처리)을 사용하기 위해 포함한다.

#include "../models/IngredientTypes.h"  // 검색 결과 DTO 타입 정의를 사용하기 위해 포함한다.

namespace ingredient  // 식재료/원재료 도메인 관련 타입을 묶는 네임스페이스.
{

// 공공 영양정보 API를 기반으로 가공식품 검색 기능을 제공하는 서비스 클래스.
class ProcessedFoodSearchService
{
  public:  // 외부에서 호출 가능한 공개 인터페이스.
    // 가공식품 검색 완료 시 결과 DTO를 전달받는 콜백 함수 타입.
    using ProcessedFoodSearchCallback =
        std::function<void(ProcessedFoodSearchResultDTO)>;

    // 기본 생성자(별도 초기화 로직 없이 컴파일러 기본 동작을 사용).
    ProcessedFoodSearchService() = default;

    // 키워드로 가공식품을 검색하고, 검색 완료 후 콜백으로 결과를 전달한다.
    void searchProcessedFoods(const std::string &keyword,
                              ProcessedFoodSearchCallback &&callback);

  private:  // 클래스 내부 구현에서만 사용하는 유틸리티 선언.
    // 문자열 양쪽 공백을 제거한 값을 반환한다.
    static std::string trim(std::string value);
    // 검색 정확도를 높이기 위해 문자열을 검색용 표준 형태로 정규화한다.
    static std::string normalizeForSearch(std::string value);
    // 환경 변수에서 지정한 키의 값을 읽어오며, 값이 없으면 빈 optional을 반환한다.
    static std::optional<std::string> getEnvValue(const char *key);
    // 로컬 파일에서 서비스 키를 읽어오며, 실패하면 빈 optional을 반환한다.
    static std::optional<std::string> getServiceKeyFromLocalFile();
};  // class ProcessedFoodSearchService

}  // namespace ingredient
