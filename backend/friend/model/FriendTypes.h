/*
 * 파일 역할 요약:
 * - 친구 기능에서 주고받는 DTO와 내부 관계 모델 타입을 정의한다.
 * - 컨트롤러/서비스/매퍼 사이의 데이터 계약(필드 구조)을 고정한다.
 */
#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace friendship
{

// 이메일 검색으로 찾은 회원 1명의 정보를 담는 DTO.
class FriendMemberDTO
{
  public:
    // 회원 고유 ID
    std::uint64_t memberId{0};
    // 회원 표시 이름
    std::string name;
    // 회원 이메일
    std::string email;
    // 회원 상태(예: ACTIVE)
    std::string status;
};

// 친구 요청 목록(수신/발신)에서 공통으로 쓰는 항목 DTO.
class FriendRequestDTO
{
  public:
    // 친구 관계(요청) ID
    std::uint64_t friendshipId{0};
    // 상대 회원 ID
    std::uint64_t memberId{0};
    // 상대 회원 이름
    std::string name;
    // 상대 회원 이메일
    std::string email;
    // 요청 상태(PENDING/ACCEPTED/REJECTED/CANCELED)
    std::string status;
    // 요청 시각
    std::string requestedAt;
    // 응답 시각(아직 응답 전이면 nullopt)
    std::optional<std::string> respondedAt;
};

// 수락된 친구 목록에서 쓰는 항목 DTO.
class FriendDTO
{
  public:
    // 친구 관계 ID
    std::uint64_t friendshipId{0};
    // 상대 회원 ID
    std::uint64_t memberId{0};
    // 상대 회원 이름
    std::string name;
    // 상대 회원 이메일
    std::string email;
    // 관계 상태
    std::string status;
    // 요청 시각
    std::string requestedAt;
    // 응답 시각(없으면 nullopt)
    std::optional<std::string> respondedAt;
};

// member_friends 테이블 1행을 서비스 로직에서 다루기 위한 내부 모델.
class FriendRelationshipModel
{
  public:
    // 친구 관계 ID(PK)
    std::uint64_t friendshipId{0};
    // 요청자 회원 ID
    std::uint64_t requesterId{0};
    // 수신자 회원 ID
    std::uint64_t addresseeId{0};
    // 관계 상태
    std::string status;
    // 요청 시각
    std::string requestedAt;
    // 응답 시각(없으면 nullopt)
    std::optional<std::string> respondedAt;
};

// 회원 검색 API 응답 DTO.
// 서비스 계층에서 HTTP 변환 전까지 공통 응답 형태로 사용한다.
class FriendSearchResultDTO
{
  public:
    // 처리 성공 여부
    bool ok{false};
    // HTTP 상태 코드
    int statusCode{400};
    // 응답 메시지
    std::string message;
    // 검색된 회원(없으면 nullopt)
    std::optional<FriendMemberDTO> member;
};

// 목록 조회 API 응답 DTO.
// friends / requests 중 어떤 컬렉션을 채울지는 호출 API 성격에 따라 달라진다.
class FriendListResultDTO
{
  public:
    // 처리 성공 여부
    bool ok{false};
    // HTTP 상태 코드
    int statusCode{400};
    // 응답 메시지
    std::string message;
    // 친구 목록 결과
    std::vector<FriendDTO> friends;
    // 요청 목록 결과
    std::vector<FriendRequestDTO> requests;
};

// 액션(요청/수락/거절/취소/삭제) API 응답 DTO.
// 변경 결과가 필요한 경우 request 필드에 최신 뷰를 담아 반환한다.
class FriendActionResultDTO
{
  public:
    // 처리 성공 여부
    bool ok{false};
    // HTTP 상태 코드
    int statusCode{400};
    // 응답 메시지
    std::string message;
    // 변경된 요청 정보(필요 시)
    std::optional<FriendRequestDTO> request;
};

}  // namespace friendship
