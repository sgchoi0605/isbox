/*
 * 파일 개요: 식재료 서비스 공통 검증/정규화 유틸 인터페이스 선언 파일이다.
 * 주요 역할: 입력값 검증과 DTO 포맷 보정 함수 시그니처를 중앙 정의해 서비스 계층 규칙 일관성을 보장한다.
 * 사용 위치: ingredient 서비스 계층 전반에서 static 유틸 형태로 include 되어 사용된다.
 */
#pragma once

#include <optional>
#include <string>

#include "ingredient/model/IngredientTypes.h"

namespace ingredient
{

// 서비스 전반에서 재사용하는 공통 검증/정규화 유틸리티.
class IngredientService_Validation
{
  public:
    // 문자열 양끝 공백 제거.
    static std::string trim(std::string value);
    // ASCII 소문자 변환.
    static std::string toLower(std::string value);
    // YYYY-MM-DD 형식 검증.
    static bool isValidDate(const std::string &value);
    // 클라이언트 storage 허용값 검증.
    static bool isAllowedClientStorage(const std::string &value);
    // 클라이언트 storage -> DB enum 변환.
    static std::string toDbStorage(const std::string &clientValue);
    // DB enum -> 클라이언트 storage 변환.
    static std::string toClientStorage(const std::string &dbValue);
    // importYn 값을 Y/N/null로 정규화.
    static std::optional<std::string> normalizeImportYn(
        const std::optional<std::string> &value);
    // YYYYMMDD 문자열을 YYYY-MM-DD로 보정.
    static std::optional<std::string> normalizeDataBaseDate(
        const std::optional<std::string> &value);
    // 조회 응답 DTO의 storage/import/date 포맷을 클라이언트 규격으로 맞춘다.
    static void normalizeIngredientForClient(IngredientDTO &ingredient);
};

}  // namespace ingredient


