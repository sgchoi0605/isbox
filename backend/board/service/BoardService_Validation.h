/*
 * 파일 역할:
 * - 게시판 서비스에서 공통으로 쓰는 문자열 정규화/검증 유틸을 선언한다.
 * - 타입 판별(share/exchange)과 DB 저장 포맷 변환 규칙을 제공한다.
 * - Query/Command에서 중복되는 전처리 로직을 한곳으로 모은다.
 */
#pragma once

#include <string>

namespace board
{

// 게시판 입력값 정규화/검증에 사용하는 공통 유틸.
class BoardService_Validation
{
  public:
    // 문자열 앞뒤 공백을 제거한다.
    // 제목/본문/검색어 입력값의 불필요한 공백 노이즈를 줄인다.
    static std::string trim(std::string value);
    // 문자열을 소문자로 변환한다.
    // 타입 비교를 대소문자 무관하게 처리하기 위한 전처리 단계다.
    static std::string toLower(std::string value);
    // 허용 게시글 타입인지 검사한다.
    // 현재 허용값은 share, exchange 두 가지다.
    static bool isAllowedType(const std::string &value);
    // 클라이언트 타입값을 DB 저장 타입값으로 변환한다.
    // share/exchange -> SHARE/EXCHANGE 매핑을 담당한다.
    static std::string toDbType(const std::string &value);
};

}  // namespace board
