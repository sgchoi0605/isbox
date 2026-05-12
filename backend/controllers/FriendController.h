#pragma once

#include <drogon/HttpTypes.h>
#include <drogon/drogon.h>

#include <cstdint>
#include <functional>
#include <optional>
#include <string>

#include "../models/FriendTypes.h"
#include "../services/FriendService.h"
#include "../services/MemberService.h"

namespace friendship
{

class FriendController
{
  public:
    explicit FriendController(auth::MemberService &memberService)
        : memberService_(memberService)
    {
    }

    void registerHandlers();

  private:
    using Callback = std::function<void(const drogon::HttpResponsePtr &)>;

    static Json::Value buildMemberJson(const FriendMemberDTO &member);
    static Json::Value buildFriendJson(const FriendDTO &friendItem);
    static Json::Value buildRequestJson(const FriendRequestDTO &request);
    static void applyCors(const drogon::HttpRequestPtr &request,
                          const drogon::HttpResponsePtr &response);
    static std::string extractSessionToken(const drogon::HttpRequestPtr &request);
    static std::optional<std::uint64_t> parseFriendshipId(
        const std::string &friendshipIdValue);

    std::optional<std::uint64_t> extractCurrentMemberId(
        const drogon::HttpRequestPtr &request);

    void handleSearchMember(const drogon::HttpRequestPtr &request, Callback &&callback);
    void handleSendRequest(const drogon::HttpRequestPtr &request, Callback &&callback);
    void handleListFriends(const drogon::HttpRequestPtr &request, Callback &&callback);
    void handleListIncomingRequests(const drogon::HttpRequestPtr &request,
                                    Callback &&callback);
    void handleListOutgoingRequests(const drogon::HttpRequestPtr &request,
                                    Callback &&callback);
    void handleAcceptRequest(const drogon::HttpRequestPtr &request,
                             Callback &&callback,
                             const std::string &friendshipIdValue);
    void handleRejectRequest(const drogon::HttpRequestPtr &request,
                             Callback &&callback,
                             const std::string &friendshipIdValue);
    void handleCancelRequest(const drogon::HttpRequestPtr &request,
                             Callback &&callback,
                             const std::string &friendshipIdValue);

    auth::MemberService &memberService_;
    FriendService friendService_;
};

}  // namespace friendship

