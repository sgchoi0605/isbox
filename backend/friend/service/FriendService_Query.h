/*
 * 파일 역할 요약:
 * - 친구 도메인의 조회 전용 유스케이스 인터페이스를 선언한다.
 * - 회원 검색, 친구 목록 조회, 수신/발신 요청 조회 메서드를 제공한다.
 */
#pragma once

#include <cstdint>
#include <string>

#include "friend/mapper/FriendMapper.h"
#include "friend/model/FriendTypes.h"

namespace friendship
{

// 친구 도메인의 조회 전용 유스케이스를 처리한다.
// 이 계층은 데이터를 읽기만 하며, 상태 변경은 수행하지 않는다.
class FriendService_Query
{
  public:
    // 조회 로직에서 사용할 Mapper 참조를 보관한다.
    // DB 접근 구현은 Mapper로 숨기고, 이 클래스는 검증/응답 DTO 조립에 집중한다.
    explicit FriendService_Query(FriendMapper &mapper) : mapper_(mapper) {}

    // 이메일로 회원을 검색한다.
    FriendSearchResultDTO searchMemberByEmail(std::uint64_t requesterId,
                                              const std::string &email);
    // 수락된 친구 목록을 조회한다.
    FriendListResultDTO listAcceptedFriends(std::uint64_t memberId);
    // 받은 요청 목록을 조회한다.
    FriendListResultDTO listIncomingRequests(std::uint64_t memberId);
    // 보낸 요청 목록을 조회한다.
    FriendListResultDTO listOutgoingRequests(std::uint64_t memberId);

  private:
    // DB 접근 레이어
    FriendMapper &mapper_;
};

}  // namespace friendship
