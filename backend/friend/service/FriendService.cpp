/*
 * 파일 역할 요약:
 * - FriendService 퍼사드의 실제 위임 로직을 구현한다.
 * - 요청 종류에 따라 Query 서비스 또는 Command 서비스로 호출을 전달한다.
 */
#include "friend/service/FriendService.h"

namespace friendship
{

// Query/Command 서비스가 같은 Mapper 인스턴스를 공유하도록 연결한다.
FriendService::FriendService() : query_(mapper_), command_(mapper_) {}

// 회원 검색 요청을 조회 서비스로 위임한다.
FriendSearchResultDTO FriendService::searchMemberByEmail(std::uint64_t requesterId,
                                                         const std::string &email)
{
    return query_.searchMemberByEmail(requesterId, email);
}

// 친구 요청 생성 요청을 커맨드 서비스로 위임한다.
FriendActionResultDTO FriendService::sendFriendRequest(std::uint64_t requesterId,
                                                       const std::string &email)
{
    return command_.sendFriendRequest(requesterId, email);
}

// 수락된 친구 목록 조회를 조회 서비스로 위임한다.
FriendListResultDTO FriendService::listAcceptedFriends(std::uint64_t memberId)
{
    return query_.listAcceptedFriends(memberId);
}

// 받은 요청 목록 조회를 조회 서비스로 위임한다.
FriendListResultDTO FriendService::listIncomingRequests(std::uint64_t memberId)
{
    return query_.listIncomingRequests(memberId);
}

// 보낸 요청 목록 조회를 조회 서비스로 위임한다.
FriendListResultDTO FriendService::listOutgoingRequests(std::uint64_t memberId)
{
    return query_.listOutgoingRequests(memberId);
}

// 요청 수락을 커맨드 서비스로 위임한다.
FriendActionResultDTO FriendService::acceptRequest(std::uint64_t memberId,
                                                   std::uint64_t friendshipId)
{
    return command_.acceptRequest(memberId, friendshipId);
}

// 요청 거절을 커맨드 서비스로 위임한다.
FriendActionResultDTO FriendService::rejectRequest(std::uint64_t memberId,
                                                   std::uint64_t friendshipId)
{
    return command_.rejectRequest(memberId, friendshipId);
}

// 요청 취소를 커맨드 서비스로 위임한다.
FriendActionResultDTO FriendService::cancelRequest(std::uint64_t memberId,
                                                   std::uint64_t friendshipId)
{
    return command_.cancelRequest(memberId, friendshipId);
}

// 친구 삭제를 커맨드 서비스로 위임한다.
FriendActionResultDTO FriendService::removeFriend(std::uint64_t memberId,
                                                  std::uint64_t friendshipId)
{
    return command_.removeFriend(memberId, friendshipId);
}

}  // namespace friendship
