/*
 * 파일 개요: 프로필 쓰기(Command) 전용 서비스 인터페이스다.
 * 주요 역할: 프로필 변경 요청 처리 계약과 이메일 검증/문자열 보정 유틸 선언.
 */
#pragma once

#include <string>

#include "member/mapper/MemberMapper.h"
#include "member/model/MemberTypes.h"
#include "member/service/MemberService_SessionStore.h"

namespace auth
{

// 프로필 쓰기 계열(수정/저장) 커맨드 서비스를 담당한다.
class MemberService_Profile_Command
{
  public:
    MemberService_Profile_Command(MemberMapper &mapper,
                                  MemberService_SessionStore &sessionStore)
        : mapper_(mapper), sessionStore_(sessionStore)
    {
    }

    // 내 프로필(이름/이메일)을 수정한다.
    // 세션 인증 후 입력 검증 및 중복 이메일 검사를 수행한다.
    UpdateProfileResultDTO updateProfile(const std::string &sessionToken,
                                         const UpdateProfileRequestDTO &request);

    // 내 Food MBTI 결과를 저장한다.
    // 배열 입력을 JSON 문자열로 직렬화해 DB 업서트에 전달한다.
    SaveFoodMbtiResultDTO saveMyFoodMbti(
        const std::string &sessionToken,
        const SaveFoodMbtiRequestDTO &request);

  private:
    // 문자열 보정/검증 유틸.
    // 이메일 비교/검증에서 동일한 정규화 규칙을 유지하기 위해 사용한다.
    static std::string trim(std::string value);
    static std::string toLower(std::string value);
    static bool isValidEmail(const std::string &email);

    // mapper_: 실제 쓰기 쿼리 실행, sessionStore_: 요청자 인증 확인
    MemberMapper &mapper_;
    MemberService_SessionStore &sessionStore_;
};

}  // namespace auth

