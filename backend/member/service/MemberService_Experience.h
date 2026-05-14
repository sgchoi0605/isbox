/*
 * 파일 개요: 경험치 지급 로직 전용 서비스 인터페이스다.
 * 주요 역할: 세션 인증 후 액션별 보상 계산과 회원 상태 갱신 계약 제공.
 */
#pragma once

#include <optional>
#include <string>

#include "member/mapper/MemberMapper.h"
#include "member/model/MemberTypes.h"
#include "member/service/MemberService_SessionStore.h"

namespace auth
{

// 사용자 행동(actionType)에 따라 경험치를 지급하는 서비스다.
class MemberService_Experience
{
  public:
    MemberService_Experience(MemberMapper &mapper, MemberService_SessionStore &sessionStore)
        : mapper_(mapper), sessionStore_(sessionStore)
    {
    }

    // 인증된 사용자의 actionType에 맞춰 경험치를 지급한다.
    // 지급량 계산, 레벨 산정, DB 반영 결과를 하나의 DTO로 반환한다.
    AwardExperienceResultDTO awardExperience(
        const std::string &sessionToken,
        const AwardExperienceRequestDTO &request);

  private:
    // 입력 정규화/규칙 매핑 공통 유틸.
    // actionType은 대소문자/공백 영향을 받지 않도록 정규화 후 비교한다.
    static std::string trim(std::string value);
    static std::string toLower(std::string value);

    // 허용된 actionType에 대한 지급 EXP를 반환한다.
    // 미정의 actionType이면 nullopt를 반환해 400 응답 분기에서 사용한다.
    static std::optional<unsigned int> resolveAwardedExpByActionType(
        const std::string &actionType);

    // mapper_: 레벨/경험치 반영, sessionStore_: 요청 사용자 인증 확인
    MemberMapper &mapper_;
    MemberService_SessionStore &sessionStore_;
};

}  // namespace auth

