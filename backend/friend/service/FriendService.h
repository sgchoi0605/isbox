/*
 * 파일 역할 요약:
 * - 친구 도메인 서비스 퍼사드 인터페이스를 선언한다.
 * - 조회(Query)와 변경(Command) 서비스를 묶어 컨트롤러가 단일 진입점으로 사용하게 한다.
 */
#pragma once

#include <cstdint>
#include <string>

#include "friend/model/FriendTypes.h"
#include "friend/service/FriendService_Command.h"
#include "friend/service/FriendService_Query.h"

namespace friendship
{

// 컨트롤러가 사용하는 친구 기능 퍼사드.
// 조회(Query)와 변경(Command)을 분리한 내부 서비스에 위임한다.
// 반환 DTO에는 HTTP 응답으로 그대로 변환 가능한 statusCode/message 규약이 포함된다.
class FriendService
{
  public:
    // Mapper를 공유하는 Query/Command 하위 서비스를 초기화한다.
    // 같은 요청 수명주기에서 조회/변경 계층이 동일한 DB 접근 규칙을 사용하도록 맞춘다.
    FriendService();

    // 이메일로 회원 1명을 검색한다.
    FriendSearchResultDTO searchMemberByEmail(std::uint64_t requesterId,
                                              const std::string &email);
    // 친구 요청을 보낸다.
    FriendActionResultDTO sendFriendRequest(std::uint64_t requesterId,
                                            const std::string &email);
    // 수락된 친구 목록을 조회한다.
    FriendListResultDTO listAcceptedFriends(std::uint64_t memberId);
    // 받은 친구 요청 목록을 조회한다.
    FriendListResultDTO listIncomingRequests(std::uint64_t memberId);
    // 보낸 친구 요청 목록을 조회한다.
    FriendListResultDTO listOutgoingRequests(std::uint64_t memberId);
    // 친구 요청을 수락한다.
    FriendActionResultDTO acceptRequest(std::uint64_t memberId,
                                        std::uint64_t friendshipId);
    // 친구 요청을 거절한다.
    FriendActionResultDTO rejectRequest(std::uint64_t memberId,
                                        std::uint64_t friendshipId);
    // 친구 요청을 취소한다.
    FriendActionResultDTO cancelRequest(std::uint64_t memberId,
                                        std::uint64_t friendshipId);
    // 수락된 친구 관계를 삭제한다.
    FriendActionResultDTO removeFriend(std::uint64_t memberId,
                                       std::uint64_t friendshipId);

  private:
    // DB 접근 레이어
    FriendMapper mapper_;
    // 조회 전용 로직
    FriendService_Query query_;
    // 변경 전용 로직
    FriendService_Command command_;
};

}  // namespace friendship
