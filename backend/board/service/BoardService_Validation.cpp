/*
 * 파일 역할:
 * - BoardService_Validation 유틸 함수 구현을 제공한다.
 * - 문자열 전처리(trim/lower)와 타입 검증/DB 포맷 변환을 담당한다.
 * - 서비스 계층 입력 검증 규칙의 중복을 줄이고 일관성을 유지한다.
 */
#include "board/service/BoardService_Validation.h"

#include <algorithm>
#include <cctype>

namespace board
{

std::string BoardService_Validation::trim(std::string value)
{
    // 앞/뒤 공백이 아닌 첫/마지막 위치를 찾아 잘라낸다.
    // 공백 문자열이어도 erase 호출만으로 안전하게 빈 문자열이 된다.
    const auto notSpace = [](unsigned char ch) { return !std::isspace(ch); };
    value.erase(value.begin(), std::find_if(value.begin(), value.end(), notSpace));
    value.erase(std::find_if(value.rbegin(), value.rend(), notSpace).base(), value.end());
    return value;
}

std::string BoardService_Validation::toLower(std::string value)
{
    // ASCII 기준 소문자 변환(타입 비교용).
    // 게시판 타입 키워드는 영문이므로 locale 비의존 변환으로 충분하다.
    std::transform(value.begin(), value.end(), value.begin(), [](unsigned char ch) {
        return static_cast<char>(std::tolower(ch));
    });
    return value;
}

bool BoardService_Validation::isAllowedType(const std::string &value)
{
    // 현재 허용 타입은 share/exchange 두 가지다.
    return value == "share" || value == "exchange";
}

std::string BoardService_Validation::toDbType(const std::string &value)
{
    // DB enum 저장 규약(대문자)으로 변환한다.
    // 호출 전 isAllowedType 검증을 거쳤다는 전제에서 동작한다.
    if (value == "share")
    {
        return "SHARE";
    }
    return "EXCHANGE";
}

}  // namespace board
