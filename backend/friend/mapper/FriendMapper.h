/*
 * 파일 역할 요약:
 * - 친구 도메인에서 사용하는 DB 접근 인터페이스를 선언한다.
 * - 회원 조회, 친구 요청 상태 변경, 친구/요청 목록 조회 시그니처를 제공한다.
 */
#pragma once

#include <drogon/orm/Row.h>

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

#include "friend/model/FriendTypes.h"

namespace friendship
{

// 친구 도메인의 DB 접근(조회/변경)을 담당하는 Mapper.
// 서비스 계층은 이 인터페이스를 통해서만 SQL 결과를 받아 비즈니스 규칙에 집중한다.
class FriendMapper
{
  public:
    FriendMapper() = default;

    // 이메일로 회원 1명을 조회한다.
    std::optional<FriendMemberDTO> findMemberByEmail(const std::string &email) const;

    // 두 회원 사이의 기존 친구 관계를 조회한다.
    std::optional<FriendRelationshipModel> findRelationship(
        std::uint64_t memberA,
        std::uint64_t memberB) const;

    // friendship_id로 친구 관계를 조회한다.
    std::optional<FriendRelationshipModel> findRelationshipById(
        std::uint64_t friendshipId) const;

    // 친구 요청을 생성하거나 기존 관계를 PENDING으로 갱신한다.
    std::optional<FriendRelationshipModel> createRequest(
        std::uint64_t requesterId,
        std::uint64_t addresseeId) const;

    // 요청 상태를 변경한다.
    // 반환값: 조건에 맞는 행이 실제로 갱신되었으면 true, 아니면 false.
    bool acceptRequest(std::uint64_t friendshipId, std::uint64_t addresseeId) const;
    bool rejectRequest(std::uint64_t friendshipId, std::uint64_t addresseeId) const;
    bool cancelRequest(std::uint64_t friendshipId, std::uint64_t requesterId) const;

    // 수락된 친구 관계를 삭제한다.
    bool removeAcceptedFriendship(std::uint64_t friendshipId,
                                  std::uint64_t memberId) const;

    // 목록 조회 API용 데이터 셋을 조회한다.
    std::vector<FriendDTO> listAcceptedFriends(std::uint64_t memberId) const;
    std::vector<FriendRequestDTO> listIncomingRequests(std::uint64_t memberId) const;
    std::vector<FriendRequestDTO> listOutgoingRequests(std::uint64_t memberId) const;

    // 특정 friendship을 "현재 회원 기준 상대방 정보 포함 요청 DTO"로 조회한다.
    std::optional<FriendRequestDTO> findRequestViewForMember(
        std::uint64_t friendshipId,
        std::uint64_t memberId) const;

  private:
    // 로컬/개발 환경에서 필요한 테이블과 제약조건을 보장한다.
    void ensureFriendsTable() const;

    // DB Row -> 도메인 DTO/모델 변환기.
    static FriendMemberDTO rowToMemberDTO(const drogon::orm::Row &row);
    static FriendRelationshipModel rowToRelationshipModel(
        const drogon::orm::Row &row);
    static FriendDTO rowToFriendDTO(const drogon::orm::Row &row);
    static FriendRequestDTO rowToRequestDTO(const drogon::orm::Row &row);

    // DB 상태값(대문자)을 클라이언트 응답 상태값(소문자)으로 변환한다.
    static std::string toClientStatus(const std::string &dbStatus);
};

}  // namespace friendship
