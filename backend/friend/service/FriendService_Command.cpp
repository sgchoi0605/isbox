/*
 * 파일 역할 요약:
 * - 친구 관계 상태를 변경하는 Command 유스케이스의 비즈니스 규칙을 구현한다.
 * - 권한 검증, 상태 검증, DB 갱신, 예외 처리, 응답 DTO 구성을 담당한다.
 */
#include "friend/service/FriendService_Command.h"

#include <drogon/orm/Exception.h>

#include "friend/service/FriendService_Policy.h"

namespace friendship
{

// 공통 처리 흐름:
// 1) 인증/입력값 검증
// 2) 권한/상태 검증
// 3) Mapper 변경 호출
// 4) DTO 구성 및 예외 처리

FriendActionResultDTO FriendService_Command::sendFriendRequest(
    std::uint64_t requesterId,
    const std::string &email)
{
    // 기본 실패값을 가진 응답 DTO를 준비한다.
    FriendActionResultDTO result;
    // 인증 사용자 ID가 없으면 401을 반환한다.
    if (requesterId == 0)
    {
        result.statusCode = 401;
        result.message = "Unauthorized.";
        return result;
    }

    // 이메일을 정규화(앞뒤 공백 제거 + 소문자화)한다.
    const auto normalizedEmail =
        FriendService_Policy::toLower(FriendService_Policy::trim(email));
    // 이메일 형식이 유효하지 않으면 400을 반환한다.
    if (!FriendService_Policy::isValidEmail(normalizedEmail))
    {
        result.statusCode = 400;
        result.message = "Invalid email format.";
        return result;
    }

    try
    {
        // 대상 회원 조회 + ACTIVE 상태 확인.
        const auto targetMember = mapper_.findMemberByEmail(normalizedEmail);
        if (!targetMember.has_value() ||
            !FriendService_Policy::isActiveMemberStatus(targetMember->status))
        {
            result.statusCode = 404;
            result.message = "Member not found.";
            return result;
        }

        // 자기 자신에게는 요청할 수 없다.
        if (targetMember->memberId == requesterId)
        {
            result.statusCode = 400;
            result.message = "You cannot send a friend request to yourself.";
            return result;
        }

        // 이미 PENDING/ACCEPTED 관계가 있으면 중복 요청을 막는다.
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

        // 새 요청을 생성(또는 기존 관계를 PENDING으로 갱신)한다.
        const auto created =
            mapper_.createRequest(requesterId, targetMember->memberId);
        if (!created.has_value())
        {
            result.statusCode = 500;
            result.message = "Failed to create friend request.";
            return result;
        }

        // 생성된 요청을 요청자 시점 뷰로 다시 조회해 응답에 넣는다.
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

FriendActionResultDTO FriendService_Command::acceptRequest(std::uint64_t memberId,
                                                           std::uint64_t friendshipId)
{
    // 기본 실패값을 가진 응답 DTO를 준비한다.
    FriendActionResultDTO result;
    // 인증 사용자 ID가 없으면 401을 반환한다.
    if (memberId == 0)
    {
        result.statusCode = 401;
        result.message = "Unauthorized.";
        return result;
    }
    // 관계 ID가 없으면 400을 반환한다.
    if (friendshipId == 0)
    {
        result.statusCode = 400;
        result.message = "Friendship id is required.";
        return result;
    }

    try
    {
        // 대상 요청 존재 여부를 확인한다.
        const auto relationship = mapper_.findRelationshipById(friendshipId);
        if (!relationship.has_value())
        {
            result.statusCode = 404;
            result.message = "Friend request not found.";
            return result;
        }
        // 수신자 본인만 수락할 수 있다.
        if (relationship->addresseeId != memberId)
        {
            result.statusCode = 403;
            result.message = "Only addressee can accept this request.";
            return result;
        }
        // PENDING 상태가 아니면 수락할 수 없다.
        if (relationship->status != "PENDING")
        {
            result.statusCode = 409;
            result.message = "Friend request is not pending.";
            return result;
        }
        // 상태를 ACCEPTED로 전환한다.
        if (!mapper_.acceptRequest(friendshipId, memberId))
        {
            result.statusCode = 409;
            result.message = "Friend request is not pending.";
            return result;
        }

        // 변경된 요청 뷰를 반환한다.
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

FriendActionResultDTO FriendService_Command::rejectRequest(std::uint64_t memberId,
                                                           std::uint64_t friendshipId)
{
    // 기본 실패값을 가진 응답 DTO를 준비한다.
    FriendActionResultDTO result;
    // 인증 사용자 ID가 없으면 401을 반환한다.
    if (memberId == 0)
    {
        result.statusCode = 401;
        result.message = "Unauthorized.";
        return result;
    }
    // 관계 ID가 없으면 400을 반환한다.
    if (friendshipId == 0)
    {
        result.statusCode = 400;
        result.message = "Friendship id is required.";
        return result;
    }

    try
    {
        // 대상 요청 존재 여부를 확인한다.
        const auto relationship = mapper_.findRelationshipById(friendshipId);
        if (!relationship.has_value())
        {
            result.statusCode = 404;
            result.message = "Friend request not found.";
            return result;
        }
        // 수신자 본인만 거절할 수 있다.
        if (relationship->addresseeId != memberId)
        {
            result.statusCode = 403;
            result.message = "Only addressee can reject this request.";
            return result;
        }
        // PENDING 상태가 아니면 거절할 수 없다.
        if (relationship->status != "PENDING")
        {
            result.statusCode = 409;
            result.message = "Friend request is not pending.";
            return result;
        }
        // 상태를 REJECTED로 전환한다.
        if (!mapper_.rejectRequest(friendshipId, memberId))
        {
            result.statusCode = 409;
            result.message = "Friend request is not pending.";
            return result;
        }

        // 변경된 요청 뷰를 반환한다.
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

FriendActionResultDTO FriendService_Command::cancelRequest(std::uint64_t memberId,
                                                           std::uint64_t friendshipId)
{
    // 기본 실패값을 가진 응답 DTO를 준비한다.
    FriendActionResultDTO result;
    // 인증 사용자 ID가 없으면 401을 반환한다.
    if (memberId == 0)
    {
        result.statusCode = 401;
        result.message = "Unauthorized.";
        return result;
    }
    // 관계 ID가 없으면 400을 반환한다.
    if (friendshipId == 0)
    {
        result.statusCode = 400;
        result.message = "Friendship id is required.";
        return result;
    }

    try
    {
        // 대상 요청 존재 여부를 확인한다.
        const auto relationship = mapper_.findRelationshipById(friendshipId);
        if (!relationship.has_value())
        {
            result.statusCode = 404;
            result.message = "Friend request not found.";
            return result;
        }
        // 요청자 본인만 취소할 수 있다.
        if (relationship->requesterId != memberId)
        {
            result.statusCode = 403;
            result.message = "Only requester can cancel this request.";
            return result;
        }
        // PENDING 상태가 아니면 취소할 수 없다.
        if (relationship->status != "PENDING")
        {
            result.statusCode = 409;
            result.message = "Friend request is not pending.";
            return result;
        }
        // 상태를 CANCELED로 전환한다.
        if (!mapper_.cancelRequest(friendshipId, memberId))
        {
            result.statusCode = 409;
            result.message = "Friend request is not pending.";
            return result;
        }

        // 변경된 요청 뷰를 반환한다.
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

FriendActionResultDTO FriendService_Command::removeFriend(std::uint64_t memberId,
                                                          std::uint64_t friendshipId)
{
    // 기본 실패값을 가진 응답 DTO를 준비한다.
    FriendActionResultDTO result;
    // 인증 사용자 ID가 없으면 401을 반환한다.
    if (memberId == 0)
    {
        result.statusCode = 401;
        result.message = "Unauthorized.";
        return result;
    }
    // 관계 ID가 없으면 400을 반환한다.
    if (friendshipId == 0)
    {
        result.statusCode = 400;
        result.message = "Friendship id is required.";
        return result;
    }

    try
    {
        // 대상 친구 관계 존재 여부를 확인한다.
        const auto relationship = mapper_.findRelationshipById(friendshipId);
        if (!relationship.has_value())
        {
            result.statusCode = 404;
            result.message = "Friendship not found.";
            return result;
        }
        // 관계 당사자만 삭제할 수 있다.
        if (relationship->requesterId != memberId &&
            relationship->addresseeId != memberId)
        {
            result.statusCode = 403;
            result.message = "Only participants can remove this friendship.";
            return result;
        }
        // 수락된 관계(ACCEPTED)만 삭제할 수 있다.
        if (relationship->status != "ACCEPTED")
        {
            result.statusCode = 409;
            result.message = "Friendship is not accepted.";
            return result;
        }
        // 관계를 실제로 삭제한다.
        if (!mapper_.removeAcceptedFriendship(friendshipId, memberId))
        {
            result.statusCode = 409;
            result.message = "Friendship is not accepted.";
            return result;
        }

        result.ok = true;
        result.statusCode = 200;
        result.message = "Friend removed.";
        return result;
    }
    catch (const drogon::orm::DrogonDbException &)
    {
        result.statusCode = 500;
        result.message = "Database error while removing friend.";
        return result;
    }
    catch (const std::exception &)
    {
        result.statusCode = 500;
        result.message = "Server error while removing friend.";
        return result;
    }
}

}  // namespace friendship
