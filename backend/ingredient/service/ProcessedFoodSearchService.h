/*
 * 파일 개요: 가공식품 검색 오케스트레이션 서비스 인터페이스 선언 파일이다.
 * 주요 역할: 검색 진입 API와 내부 구성요소(Client/Assembler/Cache/KeyProvider/Text) 조합 구조를 정의한다.
 * 사용 위치: IngredientController가 가공식품 검색 요청을 위임하는 서비스 타입으로 사용된다.
 */
#pragma once

#include <functional>
#include <string>

#include "ingredient/model/IngredientTypes.h"
#include "ingredient/service/ProcessedFoodSearchService_Assembler.h"
#include "ingredient/service/ProcessedFoodSearchService_Cache.h"
#include "ingredient/service/ProcessedFoodSearchService_Client.h"
#include "ingredient/service/ProcessedFoodSearchService_KeyProvider.h"
#include "ingredient/service/ProcessedFoodSearchService_Text.h"

namespace ingredient
{

// 가공식품 검색의 전체 흐름(검증/캐시/키조회/API호출/조립)을 오케스트레이션한다.
class ProcessedFoodSearchService
{
  public:
    using ProcessedFoodSearchCallback =
        std::function<void(ProcessedFoodSearchResultDTO)>;

    ProcessedFoodSearchService() = default;

    // 키워드 기준으로 가공식품을 비동기 검색해 콜백으로 반환한다.
    void searchProcessedFoods(const std::string &keyword,
                              ProcessedFoodSearchCallback &&callback);

  private:
    // 외부 공공 API 호출/응답 파싱 담당.
    ProcessedFoodSearchService_Client client_;
    // API 원본 row를 도메인 DTO로 조립/정렬.
    ProcessedFoodSearchService_Assembler assembler_;
    // 동일 키워드 요청 재사용을 위한 메모리 캐시.
    ProcessedFoodSearchService_Cache cache_;
    // 서비스 키 조회(env -> 로컬 파일 fallback).
    ProcessedFoodSearchService_KeyProvider keyProvider_;
    // 문자열 정규화/JSON 안전 파싱 유틸.
    ProcessedFoodSearchService_Text text_;
};

}  // namespace ingredient

