/*
 * 파일 역할 요약:
 * - 친구 요청/수락/거절/취소/삭제 같은 변경 유스케이스 인터페이스를 선언한다.
 * - 상태 전이가 필요한 쓰기 작업을 하나의 서비스로 모아 관리한다.
 */
#pragma once

#include <cstdint>
#include <string>

#include "friend/mapper/FriendMapper.h"
#include "friend/model/FriendTypes.h"

namespace friendship
{

// 친구 도메인의 변경(Command) 유스케이스를 처리한다.
// 요청 생성/수락/거절/취소/삭제처럼 상태 전이가 발생하는 작업만 담당한다.
class FriendService_Command
{
  public:
    // 변경 로직에서 사용할 Mapper 참조를 보관한다.
    // 모든 메서드는 입력 검증 + 권한/상태 검증 후 DB 변경을 수행한다.
    explicit FriendService_Command(FriendMapper &mapper) : mapper_(mapper) {}

    // 이메일 대상에게 친구 요청을 생성한다.
    FriendActionResultDTO sendFriendRequest(std::uint64_t requesterId,
                                            const std::string &email);
    // 받은 친구 요청을 수락한다.
    FriendActionResultDTO acceptRequest(std::uint64_t memberId,
                                        std::uint64_t friendshipId);
    // 받은 친구 요청을 거절한다.
    FriendActionResultDTO rejectRequest(std::uint64_t memberId,
                                        std::uint64_t friendshipId);
    // 보낸 친구 요청을 취소한다.
    FriendActionResultDTO cancelRequest(std::uint64_t memberId,
                                        std::uint64_t friendshipId);
    // 수락된 친구 관계를 제거한다.
    FriendActionResultDTO removeFriend(std::uint64_t memberId,
                                       std::uint64_t friendshipId);

  private:
    // DB 접근 레이어
    FriendMapper &mapper_;
};

}  // namespace friendship
