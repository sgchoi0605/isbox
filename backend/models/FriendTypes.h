#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace friendship
{

// 이메일 검색으로 찾은 사용자 정보 DTO.
class FriendMemberDTO
{
  public:
    std::uint64_t memberId{0};
    std::string name;
    std::string email;
    std::string status;
};

// 친구 요청 목록(수신/발신) 응답용 DTO.
class FriendRequestDTO
{
  public:
    std::uint64_t friendshipId{0};
    std::uint64_t memberId{0};
    std::string name;
    std::string email;
    std::string status;
    std::string requestedAt;
    std::optional<std::string> respondedAt;
};

// 친구 목록 응답용 DTO.
class FriendDTO
{
  public:
    std::uint64_t friendshipId{0};
    std::uint64_t memberId{0};
    std::string name;
    std::string email;
    std::string status;
    std::string requestedAt;
    std::optional<std::string> respondedAt;
};

// member_friends row를 서비스 로직에서 검사할 때 쓰는 내부 모델.
class FriendRelationshipModel
{
  public:
    std::uint64_t friendshipId{0};
    std::uint64_t requesterId{0};
    std::uint64_t addresseeId{0};
    std::string status;
    std::string requestedAt;
    std::optional<std::string> respondedAt;
};

class FriendSearchResultDTO
{
  public:
    bool ok{false};
    int statusCode{400};
    std::string message;
    std::optional<FriendMemberDTO> member;
};

class FriendListResultDTO
{
  public:
    bool ok{false};
    int statusCode{400};
    std::string message;
    std::vector<FriendDTO> friends;
    std::vector<FriendRequestDTO> requests;
};

class FriendActionResultDTO
{
  public:
    bool ok{false};
    int statusCode{400};
    std::string message;
    std::optional<FriendRequestDTO> request;
};

}  // namespace friendship

