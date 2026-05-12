#pragma once

#include <drogon/orm/Row.h>

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

#include "../models/FriendTypes.h"

namespace friendship
{

class FriendMapper
{
  public:
    FriendMapper() = default;

    // 이메일로 친구 대상 사용자를 찾는다.
    std::optional<FriendMemberDTO> findMemberByEmail(const std::string &email) const;

    // 두 사용자 쌍의 현재 관계 row를 찾는다.
    std::optional<FriendRelationshipModel> findRelationship(
        std::uint64_t memberA,
        std::uint64_t memberB) const;

    // friendship_id 기준으로 관계 row를 찾는다.
    std::optional<FriendRelationshipModel> findRelationshipById(
        std::uint64_t friendshipId) const;

    // 친구 요청을 생성(또는 과거 관계를 PENDING으로 갱신)한다.
    std::optional<FriendRelationshipModel> createRequest(
        std::uint64_t requesterId,
        std::uint64_t addresseeId) const;

    // 요청 수락/거절/취소를 반영한다.
    bool acceptRequest(std::uint64_t friendshipId, std::uint64_t addresseeId) const;
    bool rejectRequest(std::uint64_t friendshipId, std::uint64_t addresseeId) const;
    bool cancelRequest(std::uint64_t friendshipId, std::uint64_t requesterId) const;

    // 친구 목록/수신 요청/발신 요청 조회.
    std::vector<FriendDTO> listAcceptedFriends(std::uint64_t memberId) const;
    std::vector<FriendRequestDTO> listIncomingRequests(std::uint64_t memberId) const;
    std::vector<FriendRequestDTO> listOutgoingRequests(std::uint64_t memberId) const;

    // 특정 friendship를 member 관점(상대 사용자 정보 포함)으로 조회한다.
    std::optional<FriendRequestDTO> findRequestViewForMember(
        std::uint64_t friendshipId,
        std::uint64_t memberId) const;

  private:
    // 로컬 개발 환경에서 필요한 최소 스키마를 보장한다.
    void ensureFriendsTable() const;

    static FriendMemberDTO rowToMemberDTO(const drogon::orm::Row &row);
    static FriendRelationshipModel rowToRelationshipModel(
        const drogon::orm::Row &row);
    static FriendDTO rowToFriendDTO(const drogon::orm::Row &row);
    static FriendRequestDTO rowToRequestDTO(const drogon::orm::Row &row);
    static std::string toClientStatus(const std::string &dbStatus);
};

}  // namespace friendship

