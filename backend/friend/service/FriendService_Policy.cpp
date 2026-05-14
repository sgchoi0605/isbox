/*
 * 파일 역할 요약:
 * - FriendService_Policy의 문자열 정규화 및 검증 함수를 구현한다.
 * - trim/toLower/email 검사/활성 상태 검사 같은 기초 규칙의 동작을 정의한다.
 */
#include "friend/service/FriendService_Policy.h"

#include <algorithm>
#include <cctype>

namespace friendship
{

// 문자열 양끝 공백을 제거해 반환한다.
std::string FriendService_Policy::trim(std::string value)
{
    // 공백이 아닌 첫 글자를 찾는 조건 함수.
    const auto notSpace = [](unsigned char ch) { return !std::isspace(ch); };
    // 앞쪽 공백 제거.
    value.erase(value.begin(), std::find_if(value.begin(), value.end(), notSpace));
    // 뒤쪽 공백 제거.
    value.erase(std::find_if(value.rbegin(), value.rend(), notSpace).base(), value.end());
    return value;
}

// 문자열을 소문자로 변환해 반환한다.
std::string FriendService_Policy::toLower(std::string value)
{
    std::transform(value.begin(), value.end(), value.begin(), [](unsigned char ch) {
        return static_cast<char>(std::tolower(ch));
    });
    return value;
}

// 이메일 최소 형식을 검사한다.
// 조건: '@'가 가운데에 있고, '@' 뒤에 '.'이 있으며, 마지막 문자가 '.'이 아니어야 한다.
bool FriendService_Policy::isValidEmail(const std::string &email)
{
    const auto atPos = email.find('@');
    if (atPos == std::string::npos || atPos == 0 || atPos + 1 >= email.size())
    {
        return false;
    }

    const auto dotPos = email.find('.', atPos + 1);
    return dotPos != std::string::npos && dotPos + 1 < email.size();
}

// 회원 상태를 소문자로 비교해 active 여부를 판단한다.
bool FriendService_Policy::isActiveMemberStatus(const std::string &status)
{
    return toLower(status) == "active";
}

}  // namespace friendship
