#include "FriendService.h"

#include <drogon/orm/Exception.h>

#include <algorithm>
#include <cctype>

namespace friendship
{

FriendSearchResultDTO FriendService::searchMemberByEmail(std::uint64_t requesterId,
                                                         const std::string &email)
{
    FriendSearchResultDTO result;

    if (requesterId == 0)
    {
        result.statusCode = 401;
        result.message = "Unauthorized.";
        return result;
    }

    const auto normalizedEmail = toLower(trim(email));
    if (!isValidEmail(normalizedEmail))
    {
        result.statusCode = 400;
        result.message = "Invalid email format.";
        return result;
    }

    try
    {
        const auto member = mapper_.findMemberByEmail(normalizedEmail);
        if (!member.has_value() || !isActiveMemberStatus(member->status))
        {
            result.statusCode = 404;
            result.message = "Member not found.";
            return result;
        }

        result.ok = true;
        result.statusCode = 200;
        result.message = "Member found.";
        result.member = member;
        return result;
    }
    catch (const drogon::orm::DrogonDbException &)
    {
        result.statusCode = 500;
        result.message = "Database error while searching member.";
        return result;
    }
    catch (const std::exception &)
    {
        result.statusCode = 500;
        result.message = "Server error while searching member.";
        return result;
    }
}

FriendActionResultDTO FriendService::sendFriendRequest(std::uint64_t requesterId,
                                                       const std::string &email)
{
    FriendActionResultDTO result;

    if (requesterId == 0)
    {
        result.statusCode = 401;
        result.message = "Unauthorized.";
        return result;
    }

    const auto normalizedEmail = toLower(trim(email));
    if (!isValidEmail(normalizedEmail))
    {
        result.statusCode = 400;
        result.message = "Invalid email format.";
        return result;
    }

    try
    {
        const auto targetMember = mapper_.findMemberByEmail(normalizedEmail);
        if (!targetMember.has_value() || !isActiveMemberStatus(targetMember->status))
        {
            result.statusCode = 404;
            result.message = "Member not found.";
            return result;
        }

        if (targetMember->memberId == requesterId)
        {
            result.statusCode = 400;
            result.message = "You cannot send a friend request to yourself.";
            return result;
        }

        const auto relationship =
            mapper_.findRelationship(requesterId, targetMember->memberId);
        if (relationship.has_value() &&
            (relationship->status == "PENDING" ||
             relationship->status == "ACCEPTED"))
        {
            result.statusCode = 409;
            result.message = "Friend request already exists.";
            return result;
        }

        const auto created =
            mapper_.createRequest(requesterId, targetMember->memberId);
        if (!created.has_value())
        {
            result.statusCode = 500;
            result.message = "Failed to create friend request.";
            return result;
        }

        result.request =
            mapper_.findRequestViewForMember(created->friendshipId, requesterId);
        result.ok = true;
        result.statusCode = 201;
        result.message = "Friend request sent.";
        return result;
    }
    catch (const drogon::orm::DrogonDbException &)
    {
        result.statusCode = 500;
        result.message = "Database error while sending request.";
        return result;
    }
    catch (const std::exception &)
    {
        result.statusCode = 500;
        result.message = "Server error while sending request.";
        return result;
    }
}

FriendListResultDTO FriendService::listAcceptedFriends(std::uint64_t memberId)
{
    FriendListResultDTO result;

    if (memberId == 0)
    {
        result.statusCode = 401;
        result.message = "Unauthorized.";
        return result;
    }

    try
    {
        result.friends = mapper_.listAcceptedFriends(memberId);
        result.ok = true;
        result.statusCode = 200;
        result.message = "Friends loaded.";
        return result;
    }
    catch (const drogon::orm::DrogonDbException &)
    {
        result.statusCode = 500;
        result.message = "Database error while loading friends.";
        return result;
    }
    catch (const std::exception &)
    {
        result.statusCode = 500;
        result.message = "Server error while loading friends.";
        return result;
    }
}

FriendListResultDTO FriendService::listIncomingRequests(std::uint64_t memberId)
{
    FriendListResultDTO result;

    if (memberId == 0)
    {
        result.statusCode = 401;
        result.message = "Unauthorized.";
        return result;
    }

    try
    {
        result.requests = mapper_.listIncomingRequests(memberId);
        result.ok = true;
        result.statusCode = 200;
        result.message = "Incoming requests loaded.";
        return result;
    }
    catch (const drogon::orm::DrogonDbException &)
    {
        result.statusCode = 500;
        result.message = "Database error while loading incoming requests.";
        return result;
    }
    catch (const std::exception &)
    {
        result.statusCode = 500;
        result.message = "Server error while loading incoming requests.";
        return result;
    }
}

FriendListResultDTO FriendService::listOutgoingRequests(std::uint64_t memberId)
{
    FriendListResultDTO result;

    if (memberId == 0)
    {
        result.statusCode = 401;
        result.message = "Unauthorized.";
        return result;
    }

    try
    {
        result.requests = mapper_.listOutgoingRequests(memberId);
        result.ok = true;
        result.statusCode = 200;
        result.message = "Outgoing requests loaded.";
        return result;
    }
    catch (const drogon::orm::DrogonDbException &)
    {
        result.statusCode = 500;
        result.message = "Database error while loading outgoing requests.";
        return result;
    }
    catch (const std::exception &)
    {
        result.statusCode = 500;
        result.message = "Server error while loading outgoing requests.";
        return result;
    }
}

FriendActionResultDTO FriendService::acceptRequest(std::uint64_t memberId,
                                                   std::uint64_t friendshipId)
{
    FriendActionResultDTO result;

    if (memberId == 0)
    {
        result.statusCode = 401;
        result.message = "Unauthorized.";
        return result;
    }

    if (friendshipId == 0)
    {
        result.statusCode = 400;
        result.message = "Friendship id is required.";
        return result;
    }

    try
    {
        const auto relationship = mapper_.findRelationshipById(friendshipId);
        if (!relationship.has_value())
        {
            result.statusCode = 404;
            result.message = "Friend request not found.";
            return result;
        }

        if (relationship->addresseeId != memberId)
        {
            result.statusCode = 403;
            result.message = "Only addressee can accept this request.";
            return result;
        }

        if (relationship->status != "PENDING")
        {
            result.statusCode = 409;
            result.message = "Friend request is not pending.";
            return result;
        }

        if (!mapper_.acceptRequest(friendshipId, memberId))
        {
            result.statusCode = 409;
            result.message = "Friend request is not pending.";
            return result;
        }

        result.request = mapper_.findRequestViewForMember(friendshipId, memberId);
        result.ok = true;
        result.statusCode = 200;
        result.message = "Friend request accepted.";
        return result;
    }
    catch (const drogon::orm::DrogonDbException &)
    {
        result.statusCode = 500;
        result.message = "Database error while accepting request.";
        return result;
    }
    catch (const std::exception &)
    {
        result.statusCode = 500;
        result.message = "Server error while accepting request.";
        return result;
    }
}

FriendActionResultDTO FriendService::rejectRequest(std::uint64_t memberId,
                                                   std::uint64_t friendshipId)
{
    FriendActionResultDTO result;

    if (memberId == 0)
    {
        result.statusCode = 401;
        result.message = "Unauthorized.";
        return result;
    }

    if (friendshipId == 0)
    {
        result.statusCode = 400;
        result.message = "Friendship id is required.";
        return result;
    }

    try
    {
        const auto relationship = mapper_.findRelationshipById(friendshipId);
        if (!relationship.has_value())
        {
            result.statusCode = 404;
            result.message = "Friend request not found.";
            return result;
        }

        if (relationship->addresseeId != memberId)
        {
            result.statusCode = 403;
            result.message = "Only addressee can reject this request.";
            return result;
        }

        if (relationship->status != "PENDING")
        {
            result.statusCode = 409;
            result.message = "Friend request is not pending.";
            return result;
        }

        if (!mapper_.rejectRequest(friendshipId, memberId))
        {
            result.statusCode = 409;
            result.message = "Friend request is not pending.";
            return result;
        }

        result.request = mapper_.findRequestViewForMember(friendshipId, memberId);
        result.ok = true;
        result.statusCode = 200;
        result.message = "Friend request rejected.";
        return result;
    }
    catch (const drogon::orm::DrogonDbException &)
    {
        result.statusCode = 500;
        result.message = "Database error while rejecting request.";
        return result;
    }
    catch (const std::exception &)
    {
        result.statusCode = 500;
        result.message = "Server error while rejecting request.";
        return result;
    }
}

FriendActionResultDTO FriendService::cancelRequest(std::uint64_t memberId,
                                                   std::uint64_t friendshipId)
{
    FriendActionResultDTO result;

    if (memberId == 0)
    {
        result.statusCode = 401;
        result.message = "Unauthorized.";
        return result;
    }

    if (friendshipId == 0)
    {
        result.statusCode = 400;
        result.message = "Friendship id is required.";
        return result;
    }

    try
    {
        const auto relationship = mapper_.findRelationshipById(friendshipId);
        if (!relationship.has_value())
        {
            result.statusCode = 404;
            result.message = "Friend request not found.";
            return result;
        }

        if (relationship->requesterId != memberId)
        {
            result.statusCode = 403;
            result.message = "Only requester can cancel this request.";
            return result;
        }

        if (relationship->status != "PENDING")
        {
            result.statusCode = 409;
            result.message = "Friend request is not pending.";
            return result;
        }

        if (!mapper_.cancelRequest(friendshipId, memberId))
        {
            result.statusCode = 409;
            result.message = "Friend request is not pending.";
            return result;
        }

        result.request = mapper_.findRequestViewForMember(friendshipId, memberId);
        result.ok = true;
        result.statusCode = 200;
        result.message = "Friend request canceled.";
        return result;
    }
    catch (const drogon::orm::DrogonDbException &)
    {
        result.statusCode = 500;
        result.message = "Database error while canceling request.";
        return result;
    }
    catch (const std::exception &)
    {
        result.statusCode = 500;
        result.message = "Server error while canceling request.";
        return result;
    }
}

std::string FriendService::trim(std::string value)
{
    const auto notSpace = [](unsigned char ch) { return !std::isspace(ch); };

    value.erase(value.begin(),
                std::find_if(value.begin(), value.end(), notSpace));
    value.erase(
        std::find_if(value.rbegin(), value.rend(), notSpace).base(), value.end());

    return value;
}

std::string FriendService::toLower(std::string value)
{
    std::transform(value.begin(), value.end(), value.begin(), [](unsigned char ch) {
        return static_cast<char>(std::tolower(ch));
    });
    return value;
}

bool FriendService::isValidEmail(const std::string &email)
{
    const auto atPos = email.find('@');
    if (atPos == std::string::npos || atPos == 0 || atPos + 1 >= email.size())
    {
        return false;
    }

    const auto dotPos = email.find('.', atPos + 1);
    return dotPos != std::string::npos && dotPos + 1 < email.size();
}

bool FriendService::isActiveMemberStatus(const std::string &status)
{
    return toLower(status) == "active";
}

}  // namespace friendship

