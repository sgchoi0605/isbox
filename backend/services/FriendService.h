#pragma once

#include <cstdint>
#include <string>

#include "../mappers/FriendMapper.h"
#include "../models/FriendTypes.h"

namespace friendship
{

class FriendService
{
  public:
    FriendService() = default;

    FriendSearchResultDTO searchMemberByEmail(std::uint64_t requesterId,
                                              const std::string &email);

    FriendActionResultDTO sendFriendRequest(std::uint64_t requesterId,
                                            const std::string &email);

    FriendListResultDTO listAcceptedFriends(std::uint64_t memberId);
    FriendListResultDTO listIncomingRequests(std::uint64_t memberId);
    FriendListResultDTO listOutgoingRequests(std::uint64_t memberId);

    FriendActionResultDTO acceptRequest(std::uint64_t memberId,
                                        std::uint64_t friendshipId);
    FriendActionResultDTO rejectRequest(std::uint64_t memberId,
                                        std::uint64_t friendshipId);
    FriendActionResultDTO cancelRequest(std::uint64_t memberId,
                                        std::uint64_t friendshipId);

  private:
    static std::string trim(std::string value);
    static std::string toLower(std::string value);
    static bool isValidEmail(const std::string &email);
    static bool isActiveMemberStatus(const std::string &status);

    FriendMapper mapper_;
};

}  // namespace friendship

