#pragma once

#include <cstdint>
#include <functional>
#include <optional>
#include <string>

#include "../mappers/IngredientMapper.h"
#include "../models/IngredientTypes.h"

namespace ingredient
{

// 식재료 기능의 서비스 계층이다.
// 컨트롤러가 받은 요청 DTO를 바로 DB에 넘기지 않고, 이 계층에서 먼저
// 인증 여부, 필수값, 길이, 날짜 형식, 보관 위치 enum을 검증한다.
// 검증을 통과한 값만 DB 저장 형식으로 정규화해 매퍼에 전달한다.
class IngredientService
{
  public:
    IngredientService() = default;

    // 인증된 회원의 활성 식재료 목록을 조회한다.
    // DB enum이나 날짜 값은 응답 전에 프론트엔드가 쓰는 형태로 다시 변환한다.
    IngredientListResultDTO getIngredients(std::uint64_t memberId);

    // 필수 수정 가능 필드를 검증한 뒤 새 식재료를 생성한다.
    // 공공 데이터에서 가져온 선택 메타데이터도 함께 정리해서 저장한다.
    IngredientSingleResultDTO createIngredient(std::uint64_t memberId,
                                               const CreateIngredientRequestDTO &request);

    // 회원이 소유한 식재료의 수정 가능 필드를 갱신한다.
    // 공공 데이터 원본 정보는 수정 대상에서 제외하고, 사용자가 직접 바꾸는 필드만 갱신한다.
    IngredientSingleResultDTO updateIngredient(std::uint64_t memberId,
                                               std::uint64_t ingredientId,
                                               const UpdateIngredientRequestDTO &request);

    // 회원이 소유한 식재료를 소프트 삭제한다.
    // 실제 행 삭제 대신 상태값을 바꿔 목록 조회에서 제외되도록 한다.
    IngredientDeleteResultDTO deleteIngredient(std::uint64_t memberId,
                                               std::uint64_t ingredientId);

    // 가공식품 검색 결과를 비동기로 돌려주기 위한 콜백 타입이다.
    // Drogon HTTP 클라이언트의 비동기 응답을 서비스 결과 DTO로 감싸서 컨트롤러에 전달한다.
    using ProcessedFoodSearchCallback =
        std::function<void(ProcessedFoodSearchResultDTO)>;

    // 키워드로 공공 영양 API를 검색한다.
    // 외부 API 호출이므로 성공/실패 모두 callback으로 반환한다.
    void searchProcessedFoods(const std::string &keyword,
                              ProcessedFoodSearchCallback &&callback);

  private:
    // 문자열 앞뒤 공백을 제거한다.
    static std::string trim(std::string value);

    // 문자열을 소문자로 변환한다.
    static std::string toLower(std::string value);

    // YYYY-MM-DD 날짜 형식을 엄격하게 검증한다.
    // 현재는 실제 달력 유효성보다 API 계약의 문자열 형식 검증에 집중한다.
    static bool isValidDate(const std::string &value);

    // 클라이언트 보관 위치 열거형 값이 허용되는지 검증한다.
    // 허용 값: fridge, freezer, roomtemp, other.
    static bool isAllowedClientStorage(const std::string &value);

    // 클라이언트 보관 위치 값을 DB 열거형 호환 값으로 변환한다.
    // 예: roomtemp -> ROOM_TEMP.
    static std::string toDbStorage(const std::string &clientValue);

    // DB 보관 위치 열거형 값을 클라이언트 응답 값으로 변환한다.
    // 예: ROOM_TEMP -> roomTemp.
    static std::string toClientStorage(const std::string &dbValue);

    // import_yn 필드를 Y/N 또는 nullopt로 정규화한다.
    // 공공 API가 소문자나 공백 포함 값을 내려줘도 저장 전 일관된 값으로 맞춘다.
    static std::optional<std::string> normalizeImportYn(
        const std::optional<std::string> &value);

    // 매퍼 결과 값을 클라이언트 응답 형식으로 정규화한다.
    // DB 내부 표현을 그대로 노출하지 않기 위한 마지막 응답 가공 단계다.
    static void normalizeIngredientForClient(IngredientDTO &ingredient);

    // 비어 있지 않은 환경변수 값을 읽는다.
    // 공공 API 키를 배포 환경에서 주입할 때 사용한다.
    static std::optional<std::string> getEnvValue(const char *key);
    // 로컬 JSON 파일에서 공공 영양 API 키를 읽는다.
    // 개발 환경에서 환경변수 없이 실행할 수 있도록 보조 경로를 제공한다.
    static std::optional<std::string> getServiceKeyFromLocalFile();

    // 데이터 접근 객체다.
    IngredientMapper mapper_;
};

}  // 식재료 네임스페이스

