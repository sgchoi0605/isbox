#pragma once

// 고정 폭 정수 타입(std::uint64_t)을 사용하기 위해 포함한다.
#include <cstdint>
// 문자열(std::string)을 사용하기 위해 포함한다.
#include <string>

// DB 접근과 조회/변경 로직을 담당하는 매퍼를 사용하기 위해 포함한다.
#include "../mappers/FriendMapper.h"
// 서비스 계층의 입력/출력 DTO 타입을 사용하기 위해 포함한다.
#include "../models/FriendTypes.h"

// 친구 관련 서비스 코드를 한 범위로 묶기 위한 네임스페이스이다.
namespace friendship
{

class FriendService
{
  public:
    // 별도 초기화가 필요 없는 기본 생성자이다.
    FriendService() = default;

    // 이메일로 회원을 검색한다.
    FriendSearchResultDTO searchMemberByEmail(std::uint64_t requesterId,
                                              const std::string &email);

    // 이메일 대상에게 친구 요청을 보낸다.
    FriendActionResultDTO sendFriendRequest(std::uint64_t requesterId,
                                            const std::string &email);

    // 수락된 친구 목록을 조회한다.
    FriendListResultDTO listAcceptedFriends(std::uint64_t memberId);
    // 내가 받은 친구 요청 목록을 조회한다.
    FriendListResultDTO listIncomingRequests(std::uint64_t memberId);
    // 내가 보낸 친구 요청 목록을 조회한다.
    FriendListResultDTO listOutgoingRequests(std::uint64_t memberId);

    // 받은 친구 요청을 수락한다.
    FriendActionResultDTO acceptRequest(std::uint64_t memberId,
                                        std::uint64_t friendshipId);
    // 받은 친구 요청을 거절한다.
    FriendActionResultDTO rejectRequest(std::uint64_t memberId,
                                        std::uint64_t friendshipId);
    // 보낸 친구 요청을 취소한다.
    FriendActionResultDTO cancelRequest(std::uint64_t memberId,
                                        std::uint64_t friendshipId);

  private:
    // 문자열 앞뒤 공백을 제거한다.
    static std::string trim(std::string value);
    // 문자열을 소문자로 정규화한다.
    static std::string toLower(std::string value);
    // 이메일 형식이 최소 조건을 만족하는지 검사한다.
    static bool isValidEmail(const std::string &email);
    // 회원 상태가 활성 상태인지 검사한다.
    static bool isActiveMemberStatus(const std::string &status);

    // 친구 관련 DB 작업을 위임하는 매퍼 인스턴스이다.
    FriendMapper mapper_;
};

}  // namespace friendship

