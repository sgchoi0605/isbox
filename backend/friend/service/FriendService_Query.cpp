/*
 * 파일 역할 요약:
 * - 친구 조회 유스케이스의 인증/입력 검증과 데이터 조회 흐름을 구현한다.
 * - 조회 결과 DTO 구성과 예외 처리 정책(오류 코드/메시지)을 일관되게 적용한다.
 */
#include "friend/service/FriendService_Query.h"

#include <drogon/orm/Exception.h>

#include "friend/service/FriendService_Policy.h"

namespace friendship
{

// 공통 처리 흐름:
// 1) 인증/입력값 검증
// 2) Mapper 호출
// 3) 결과 DTO 구성
// 4) DB/일반 예외를 500으로 변환

FriendSearchResultDTO FriendService_Query::searchMemberByEmail(
    std::uint64_t requesterId,
    const std::string &email)
{
    // 기본 실패값을 가진 응답 DTO를 준비한다.
    FriendSearchResultDTO result;
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
        // 이메일로 회원을 조회하고 ACTIVE 상태인지 확인한다.
        const auto member = mapper_.findMemberByEmail(normalizedEmail);
        if (!member.has_value() ||
            !FriendService_Policy::isActiveMemberStatus(member->status))
        {
            result.statusCode = 404;
            result.message = "Member not found.";
            return result;
        }

        // 조회 성공 결과를 채운다.
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

FriendListResultDTO FriendService_Query::listAcceptedFriends(std::uint64_t memberId)
{
    // 기본 실패값을 가진 응답 DTO를 준비한다.
    FriendListResultDTO result;
    // 인증 사용자 ID가 없으면 401을 반환한다.
    if (memberId == 0)
    {
        result.statusCode = 401;
        result.message = "Unauthorized.";
        return result;
    }

    try
    {
        // 수락된 친구 목록을 조회한다.
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

FriendListResultDTO FriendService_Query::listIncomingRequests(std::uint64_t memberId)
{
    // 기본 실패값을 가진 응답 DTO를 준비한다.
    FriendListResultDTO result;
    // 인증 사용자 ID가 없으면 401을 반환한다.
    if (memberId == 0)
    {
        result.statusCode = 401;
        result.message = "Unauthorized.";
        return result;
    }

    try
    {
        // 받은 친구 요청 목록을 조회한다.
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

FriendListResultDTO FriendService_Query::listOutgoingRequests(std::uint64_t memberId)
{
    // 기본 실패값을 가진 응답 DTO를 준비한다.
    FriendListResultDTO result;
    // 인증 사용자 ID가 없으면 401을 반환한다.
    if (memberId == 0)
    {
        result.statusCode = 401;
        result.message = "Unauthorized.";
        return result;
    }

    try
    {
        // 보낸 친구 요청 목록을 조회한다.
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

}  // namespace friendship
