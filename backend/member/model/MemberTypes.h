/*
 * 파일 개요: 회원 도메인에서 사용하는 요청/응답/DB 모델 DTO 타입 모음이다.
 * 주요 역할: 계층 간 데이터 전달 구조를 일관되게 정의하고 민감정보 포함 범위를 구분.
 */
#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace auth
{

// 로그인 요청 본문을 담는 DTO다.
// 컨트롤러가 JSON을 파싱해 서비스로 전달할 때 사용한다.
class LoginRequestDTO
{
  public:
    std::string email;
    std::string password;
};

// 회원가입 요청 본문을 담는 DTO다.
// 약관 동의 여부와 비밀번호 확인값까지 함께 전달한다.
class SignupRequestDTO
{
  public:
    std::string name;
    std::string email;
    std::string password;
    std::string confirmPassword;
    bool agreeTerms{false};
};

// 내 프로필 수정 요청 DTO다.
// 현재 구조에서는 이름/이메일 변경만 허용한다.
class UpdateProfileRequestDTO
{
  public:
    std::string name;
    std::string email;
};

// 비밀번호 변경 요청 DTO다.
// 현재 비밀번호 검증과 새 비밀번호 확인용 필드를 모두 포함한다.
class ChangePasswordRequestDTO
{
  public:
    std::string currentPassword;
    std::string newPassword;
    std::string confirmPassword;
};

// DB 저장용 Food MBTI 모델(원본 구조)이다.
// JSON 문자열 필드를 포함해 테이블 구조와 1:1에 가깝게 맞춘다.
class FoodMbtiModel
{
  public:
    std::uint64_t memberId{0};
    std::string type;
    std::string title;
    std::string description;
    std::string traitsJson;
    std::string recommendedFoodsJson;
    std::string completedAt;
    std::string updatedAt;
};

// API 응답용 Food MBTI DTO다.
// DB의 JSON 문자열(traits/recommendedFoods)을 파싱한 배열 형태를 제공한다.
class FoodMbtiDTO
{
  public:
    std::string type;
    std::string title;
    std::string description;
    std::vector<std::string> traits;
    std::vector<std::string> recommendedFoods;
    std::string completedAt;
};

// Food MBTI 저장 요청 DTO다.
// 클라이언트 입력은 배열로 받고, 저장 시 서비스에서 JSON 문자열로 직렬화한다.
class SaveFoodMbtiRequestDTO
{
  public:
    std::string type;
    std::string title;
    std::string description;
    std::vector<std::string> traits;
    std::vector<std::string> recommendedFoods;
    std::optional<std::string> completedAt;
};

// Food MBTI 저장 결과 DTO다.
// 저장 성공 여부와 HTTP 상태코드, 최신 MBTI 응답 데이터를 함께 담는다.
class SaveFoodMbtiResultDTO
{
  public:
    bool ok{false};
    int statusCode{400};
    std::string message;
    std::optional<FoodMbtiDTO> foodMbti;
};

// 프로필 단건 응답 DTO다.
// 본인 여부(isMe)와 선택적 Food MBTI 정보를 함께 노출한다.
class MemberProfileDTO
{
  public:
    std::uint64_t memberId{0};
    std::string name;
    std::string email;
    unsigned int level{1};
    unsigned int exp{0};
    bool isMe{false};
    std::optional<FoodMbtiDTO> foodMbti;
};

// 프로필 조회 결과 DTO다.
// 조회 실패/권한 오류/성공을 statusCode + message + optional payload로 표현한다.
class MemberProfileResultDTO
{
  public:
    bool ok{false};
    int statusCode{400};
    std::string message;
    std::optional<MemberProfileDTO> profile;
};

// DB members row 모델이다.
// 내부 로직용이라 passwordHash 같은 민감정보를 포함할 수 있다.
class MemberModel
{
  public:
    std::uint64_t memberId{0};
    std::string email;
    std::string passwordHash;
    std::string name;
    std::string role;
    std::string status;
    unsigned int level{1};
    unsigned int exp{0};
    std::optional<std::string> lastLoginAt;
};

// API 응답용 회원 DTO다.
// 외부 노출 대상이므로 민감정보(passwordHash)는 제외한다.
class MemberDTO
{
  public:
    std::uint64_t memberId{0};
    std::string email;
    std::string name;
    std::string role;
    std::string status;
    unsigned int level{1};
    unsigned int exp{0};
    std::optional<std::string> lastLoginAt;
};

// 로그인/회원가입 결과 DTO다.
// 성공 시 회원 정보와 세션 토큰을 함께 반환한다.
class AuthResultDTO
{
  public:
    bool ok{false};
    int statusCode{400};
    std::string message;
    std::optional<MemberDTO> member;
    std::optional<std::string> sessionToken;
};

// 프로필 수정 결과 DTO다.
// 성공 시 수정된 회원 기본 정보를 다시 내려준다.
class UpdateProfileResultDTO
{
  public:
    bool ok{false};
    int statusCode{400};
    std::string message;
    std::optional<MemberDTO> member;
};

// 비밀번호 변경 결과 DTO다.
// 보안상 회원 상세정보는 포함하지 않고 성공/실패 메시지만 전달한다.
class ChangePasswordResultDTO
{
  public:
    bool ok{false};
    int statusCode{400};
    std::string message;
};

// 경험치 지급 요청 DTO다.
// 어떤 사용자 행동(actionType)으로 보상을 요청했는지를 전달한다.
class AwardExperienceRequestDTO
{
  public:
    std::string actionType;
};

// 경험치 지급 결과 DTO다.
// 지급량, 이전/신규 레벨, 반영된 회원 상태를 함께 전달한다.
class AwardExperienceResultDTO
{
  public:
    bool ok{false};
    int statusCode{400};
    std::string message;
    unsigned int awardedExp{0};
    unsigned int previousLevel{1};
    unsigned int newLevel{1};
    std::optional<MemberDTO> member;
};

}  // namespace auth

