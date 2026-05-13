#include "FriendService.h"

// Drogon ORM 예외 타입(DrogonDbException)을 잡기 위해 포함한다.
#include <drogon/orm/Exception.h>

// std::find_if, std::transform 같은 알고리즘 유틸을 사용하기 위해 포함한다.
#include <algorithm>
// std::isspace, std::tolower 문자 처리 함수를 사용하기 위해 포함한다.
#include <cctype>

// 친구 서비스 구현 코드를 네임스페이스 범위로 제한한다.
namespace friendship
{

FriendSearchResultDTO FriendService::searchMemberByEmail(std::uint64_t requesterId,
                                                         const std::string &email)
{
    // API 응답으로 사용할 기본 결과 객체를 생성한다.
    FriendSearchResultDTO result;

    // 인증되지 않은 요청(회원 ID 없음)은 즉시 거부한다.
    if (requesterId == 0)
    {
        // HTTP 401 Unauthorized 의미의 코드와 메시지를 설정한다.
        result.statusCode = 401;
        result.message = "Unauthorized.";
        // 실패 응답을 반환한다.
        return result;
    }

    // 입력 이메일의 앞뒤 공백 제거 + 소문자 변환으로 비교 가능한 형태로 정규화한다.
    const auto normalizedEmail = toLower(trim(email));
    // 이메일이 유효한 최소 형식인지 검사한다.
    if (!isValidEmail(normalizedEmail))
    {
        // 형식 오류이므로 400 Bad Request를 반환한다.
        result.statusCode = 400;
        result.message = "Invalid email format.";
        return result;
    }

    // DB 조회/변환 과정에서 발생 가능한 예외를 처리하기 위해 try 블록을 사용한다.
    try
    {
        // 정규화된 이메일로 회원 정보를 조회한다.
        const auto member = mapper_.findMemberByEmail(normalizedEmail);
        // 회원이 없거나 활성 상태가 아니면 조회 실패로 처리한다.
        if (!member.has_value() || !isActiveMemberStatus(member->status))
        {
            result.statusCode = 404;
            result.message = "Member not found.";
            return result;
        }

        // 정상 조회 결과를 채운다.
        result.ok = true;
        result.statusCode = 200;
        result.message = "Member found.";
        result.member = member;
        return result;
    }
    // Drogon DB 계층 예외는 데이터베이스 오류로 분류한다.
    catch (const drogon::orm::DrogonDbException &)
    {
        result.statusCode = 500;
        result.message = "Database error while searching member.";
        return result;
    }
    // 그 외 표준 예외는 서버 내부 오류로 분류한다.
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
    // API 응답으로 사용할 기본 결과 객체를 생성한다.
    FriendActionResultDTO result;

    // 인증되지 않은 요청은 처리하지 않는다.
    if (requesterId == 0)
    {
        result.statusCode = 401;
        result.message = "Unauthorized.";
        return result;
    }

    // 입력 이메일을 비교 가능한 형태(트림 + 소문자)로 정규화한다.
    const auto normalizedEmail = toLower(trim(email));
    // 이메일 형식이 유효하지 않으면 요청을 거부한다.
    if (!isValidEmail(normalizedEmail))
    {
        result.statusCode = 400;
        result.message = "Invalid email format.";
        return result;
    }

    // DB 조회/저장 중 발생 가능한 예외를 처리한다.
    try
    {
        // 친구 요청 대상 회원을 이메일로 조회한다.
        const auto targetMember = mapper_.findMemberByEmail(normalizedEmail);
        // 대상 회원이 없거나 비활성 상태면 404를 반환한다.
        if (!targetMember.has_value() || !isActiveMemberStatus(targetMember->status))
        {
            result.statusCode = 404;
            result.message = "Member not found.";
            return result;
        }

        // 본인에게 친구 요청을 보내는 행위를 차단한다.
        if (targetMember->memberId == requesterId)
        {
            result.statusCode = 400;
            result.message = "You cannot send a friend request to yourself.";
            return result;
        }

        // 요청자-대상자 간 기존 관계가 있는지 먼저 조회한다.
        const auto relationship =
            mapper_.findRelationship(requesterId, targetMember->memberId);
        // 이미 대기 중(PENDING) 또는 친구 상태(ACCEPTED)면 중복 요청으로 거부한다.
        if (relationship.has_value() &&
            (relationship->status == "PENDING" ||
             relationship->status == "ACCEPTED"))
        {
            result.statusCode = 409;
            result.message = "Friend request already exists.";
            return result;
        }

        // 새 친구 요청 레코드를 생성한다.
        const auto created =
            mapper_.createRequest(requesterId, targetMember->memberId);
        // 생성 결과가 없으면 내부 오류로 처리한다.
        if (!created.has_value())
        {
            result.statusCode = 500;
            result.message = "Failed to create friend request.";
            return result;
        }

        // 생성된 요청 ID 기준으로 응답용 뷰 데이터를 재조회한다.
        result.request =
            mapper_.findRequestViewForMember(created->friendshipId, requesterId);
        // 생성 성공 응답을 설정한다.
        result.ok = true;
        result.statusCode = 201;
        result.message = "Friend request sent.";
        return result;
    }
    // DB 예외는 데이터베이스 오류로 처리한다.
    catch (const drogon::orm::DrogonDbException &)
    {
        result.statusCode = 500;
        result.message = "Database error while sending request.";
        return result;
    }
    // 그 외 예외는 일반 서버 오류로 처리한다.
    catch (const std::exception &)
    {
        result.statusCode = 500;
        result.message = "Server error while sending request.";
        return result;
    }
}

FriendListResultDTO FriendService::listAcceptedFriends(std::uint64_t memberId)
{
    // 목록 조회 응답 객체를 생성한다.
    FriendListResultDTO result;

    // 인증 정보가 없으면 조회를 거부한다.
    if (memberId == 0)
    {
        result.statusCode = 401;
        result.message = "Unauthorized.";
        return result;
    }

    // DB 조회 예외를 처리한다.
    try
    {
        // 수락된 친구 목록을 매퍼에서 읽어온다.
        result.friends = mapper_.listAcceptedFriends(memberId);
        // 성공 응답 필드를 채운다.
        result.ok = true;
        result.statusCode = 200;
        result.message = "Friends loaded.";
        return result;
    }
    // DB 오류를 별도로 분리해 메시지를 반환한다.
    catch (const drogon::orm::DrogonDbException &)
    {
        result.statusCode = 500;
        result.message = "Database error while loading friends.";
        return result;
    }
    // 나머지 예외는 서버 오류로 처리한다.
    catch (const std::exception &)
    {
        result.statusCode = 500;
        result.message = "Server error while loading friends.";
        return result;
    }
}

FriendListResultDTO FriendService::listIncomingRequests(std::uint64_t memberId)
{
    // 목록 조회 응답 객체를 생성한다.
    FriendListResultDTO result;

    // 인증 정보가 없으면 조회를 거부한다.
    if (memberId == 0)
    {
        result.statusCode = 401;
        result.message = "Unauthorized.";
        return result;
    }

    // DB 조회 예외를 처리한다.
    try
    {
        // 내가 받은 친구 요청 목록을 조회한다.
        result.requests = mapper_.listIncomingRequests(memberId);
        // 성공 응답을 설정한다.
        result.ok = true;
        result.statusCode = 200;
        result.message = "Incoming requests loaded.";
        return result;
    }
    // DB 오류를 반환한다.
    catch (const drogon::orm::DrogonDbException &)
    {
        result.statusCode = 500;
        result.message = "Database error while loading incoming requests.";
        return result;
    }
    // 일반 서버 오류를 반환한다.
    catch (const std::exception &)
    {
        result.statusCode = 500;
        result.message = "Server error while loading incoming requests.";
        return result;
    }
}

FriendListResultDTO FriendService::listOutgoingRequests(std::uint64_t memberId)
{
    // 목록 조회 응답 객체를 생성한다.
    FriendListResultDTO result;

    // 인증 정보가 없으면 조회를 거부한다.
    if (memberId == 0)
    {
        result.statusCode = 401;
        result.message = "Unauthorized.";
        return result;
    }

    // DB 조회 예외를 처리한다.
    try
    {
        // 내가 보낸 친구 요청 목록을 조회한다.
        result.requests = mapper_.listOutgoingRequests(memberId);
        // 성공 응답을 설정한다.
        result.ok = true;
        result.statusCode = 200;
        result.message = "Outgoing requests loaded.";
        return result;
    }
    // DB 오류를 반환한다.
    catch (const drogon::orm::DrogonDbException &)
    {
        result.statusCode = 500;
        result.message = "Database error while loading outgoing requests.";
        return result;
    }
    // 일반 서버 오류를 반환한다.
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
    // 액션 처리 응답 객체를 생성한다.
    FriendActionResultDTO result;

    // 인증 정보가 없으면 요청을 거부한다.
    if (memberId == 0)
    {
        result.statusCode = 401;
        result.message = "Unauthorized.";
        return result;
    }

    // 처리 대상 친구 관계 ID가 없으면 잘못된 요청이다.
    if (friendshipId == 0)
    {
        result.statusCode = 400;
        result.message = "Friendship id is required.";
        return result;
    }

    // DB 조회/갱신 예외를 처리한다.
    try
    {
        // 친구 요청 레코드를 ID로 조회한다.
        const auto relationship = mapper_.findRelationshipById(friendshipId);
        // 해당 요청이 없으면 404를 반환한다.
        if (!relationship.has_value())
        {
            result.statusCode = 404;
            result.message = "Friend request not found.";
            return result;
        }

        // 수신자(addressee) 본인만 수락할 수 있도록 권한을 검증한다.
        if (relationship->addresseeId != memberId)
        {
            result.statusCode = 403;
            result.message = "Only addressee can accept this request.";
            return result;
        }

        // 상태가 PENDING이 아닐 경우 수락할 수 없다.
        if (relationship->status != "PENDING")
        {
            result.statusCode = 409;
            result.message = "Friend request is not pending.";
            return result;
        }

        // 실제 수락 업데이트를 수행한다(낙관적 충돌 포함).
        if (!mapper_.acceptRequest(friendshipId, memberId))
        {
            result.statusCode = 409;
            result.message = "Friend request is not pending.";
            return result;
        }

        // 변경된 요청 뷰를 응답에 담기 위해 재조회한다.
        result.request = mapper_.findRequestViewForMember(friendshipId, memberId);
        // 성공 응답을 설정한다.
        result.ok = true;
        result.statusCode = 200;
        result.message = "Friend request accepted.";
        return result;
    }
    // DB 오류를 반환한다.
    catch (const drogon::orm::DrogonDbException &)
    {
        result.statusCode = 500;
        result.message = "Database error while accepting request.";
        return result;
    }
    // 일반 서버 오류를 반환한다.
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
    // 액션 처리 응답 객체를 생성한다.
    FriendActionResultDTO result;

    // 인증 정보가 없으면 요청을 거부한다.
    if (memberId == 0)
    {
        result.statusCode = 401;
        result.message = "Unauthorized.";
        return result;
    }

    // 처리 대상 친구 관계 ID가 없으면 잘못된 요청이다.
    if (friendshipId == 0)
    {
        result.statusCode = 400;
        result.message = "Friendship id is required.";
        return result;
    }

    // DB 조회/갱신 예외를 처리한다.
    try
    {
        // 친구 요청 레코드를 ID로 조회한다.
        const auto relationship = mapper_.findRelationshipById(friendshipId);
        // 해당 요청이 없으면 404를 반환한다.
        if (!relationship.has_value())
        {
            result.statusCode = 404;
            result.message = "Friend request not found.";
            return result;
        }

        // 수신자(addressee) 본인만 거절할 수 있도록 검증한다.
        if (relationship->addresseeId != memberId)
        {
            result.statusCode = 403;
            result.message = "Only addressee can reject this request.";
            return result;
        }

        // 상태가 PENDING이 아닐 경우 거절할 수 없다.
        if (relationship->status != "PENDING")
        {
            result.statusCode = 409;
            result.message = "Friend request is not pending.";
            return result;
        }

        // 실제 거절 업데이트를 수행한다.
        if (!mapper_.rejectRequest(friendshipId, memberId))
        {
            result.statusCode = 409;
            result.message = "Friend request is not pending.";
            return result;
        }

        // 변경된 요청 뷰를 응답에 담기 위해 재조회한다.
        result.request = mapper_.findRequestViewForMember(friendshipId, memberId);
        // 성공 응답을 설정한다.
        result.ok = true;
        result.statusCode = 200;
        result.message = "Friend request rejected.";
        return result;
    }
    // DB 오류를 반환한다.
    catch (const drogon::orm::DrogonDbException &)
    {
        result.statusCode = 500;
        result.message = "Database error while rejecting request.";
        return result;
    }
    // 일반 서버 오류를 반환한다.
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
    // 액션 처리 응답 객체를 생성한다.
    FriendActionResultDTO result;

    // 인증 정보가 없으면 요청을 거부한다.
    if (memberId == 0)
    {
        result.statusCode = 401;
        result.message = "Unauthorized.";
        return result;
    }

    // 처리 대상 친구 관계 ID가 없으면 잘못된 요청이다.
    if (friendshipId == 0)
    {
        result.statusCode = 400;
        result.message = "Friendship id is required.";
        return result;
    }

    // DB 조회/갱신 예외를 처리한다.
    try
    {
        // 친구 요청 레코드를 ID로 조회한다.
        const auto relationship = mapper_.findRelationshipById(friendshipId);
        // 해당 요청이 없으면 404를 반환한다.
        if (!relationship.has_value())
        {
            result.statusCode = 404;
            result.message = "Friend request not found.";
            return result;
        }

        // 요청자(requester) 본인만 취소할 수 있도록 권한을 검증한다.
        if (relationship->requesterId != memberId)
        {
            result.statusCode = 403;
            result.message = "Only requester can cancel this request.";
            return result;
        }

        // 상태가 PENDING이 아닐 경우 취소할 수 없다.
        if (relationship->status != "PENDING")
        {
            result.statusCode = 409;
            result.message = "Friend request is not pending.";
            return result;
        }

        // 실제 취소 업데이트를 수행한다.
        if (!mapper_.cancelRequest(friendshipId, memberId))
        {
            result.statusCode = 409;
            result.message = "Friend request is not pending.";
            return result;
        }

        // 변경된 요청 뷰를 응답에 담기 위해 재조회한다.
        result.request = mapper_.findRequestViewForMember(friendshipId, memberId);
        // 성공 응답을 설정한다.
        result.ok = true;
        result.statusCode = 200;
        result.message = "Friend request canceled.";
        return result;
    }
    // DB 오류를 반환한다.
    catch (const drogon::orm::DrogonDbException &)
    {
        result.statusCode = 500;
        result.message = "Database error while canceling request.";
        return result;
    }
    // 일반 서버 오류를 반환한다.
    catch (const std::exception &)
    {
        result.statusCode = 500;
        result.message = "Server error while canceling request.";
        return result;
    }
}

std::string FriendService::trim(std::string value)
{
    // 공백 문자인지 판별하는 람다의 부정으로 "공백이 아닌 문자"를 찾는다.
    const auto notSpace = [](unsigned char ch) { return !std::isspace(ch); };

    // 문자열 앞쪽에서 첫 번째 비공백 위치까지를 삭제해 왼쪽 공백을 제거한다.
    value.erase(value.begin(),
                std::find_if(value.begin(), value.end(), notSpace));
    // 문자열 뒤쪽에서 첫 번째 비공백 위치 다음까지 남기고 오른쪽 공백을 제거한다.
    value.erase(
        std::find_if(value.rbegin(), value.rend(), notSpace).base(), value.end());

    // 앞뒤 공백이 제거된 문자열을 반환한다.
    return value;
}

std::string FriendService::toLower(std::string value)
{
    // 문자열의 각 문자를 소문자로 변환해 같은 버퍼에 다시 기록한다.
    std::transform(value.begin(), value.end(), value.begin(), [](unsigned char ch) {
        return static_cast<char>(std::tolower(ch));
    });
    // 소문자 정규화 결과를 반환한다.
    return value;
}

bool FriendService::isValidEmail(const std::string &email)
{
    // '@' 위치를 찾는다.
    const auto atPos = email.find('@');
    // '@'가 없거나, 맨 앞이거나, '@' 뒤에 문자가 없으면 유효하지 않다.
    if (atPos == std::string::npos || atPos == 0 || atPos + 1 >= email.size())
    {
        return false;
    }

    // 도메인 부분에 '.'이 있는지 검사한다.
    const auto dotPos = email.find('.', atPos + 1);
    // '.'이 존재하고 마지막 문자가 아니어야 유효하다고 본다.
    return dotPos != std::string::npos && dotPos + 1 < email.size();
}

bool FriendService::isActiveMemberStatus(const std::string &status)
{
    // 상태 문자열을 소문자로 정규화한 뒤 active 여부를 비교한다.
    return toLower(status) == "active";
}

}  // namespace friendship

