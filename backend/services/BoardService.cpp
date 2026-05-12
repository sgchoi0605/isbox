#include "BoardService.h"

#include <drogon/orm/Exception.h>

#include <algorithm>
#include <cctype>

namespace board
{

BoardListResultDTO BoardService::getPosts(
    const std::optional<std::string> &typeFilter)
{
    BoardListResultDTO result;

    // 필요한 경우 클라이언트 필터 값을 DB 열거형 필터로 변환한다.
    std::optional<std::string> dbTypeFilter;
    if (typeFilter.has_value())
    {
        const auto normalized = toLower(trim(*typeFilter));

        // 빈 필터나 "all"은 유형 조건 없이 조회한다는 의미다.
        if (!normalized.empty() && normalized != "all")
        {
            // 지원하지 않는 필터 값은 DB 조회 전에 거절한다.
            if (!isAllowedType(normalized))
            {
                result.statusCode = 400;
                result.message = "Invalid post type filter.";
                return result;
            }

            dbTypeFilter = toDbType(normalized);
        }
    }

    try
    {
        // 실제 DB 조회는 매퍼가 담당한다.
        result.posts = mapper_.findPosts(dbTypeFilter);
        result.ok = true;
        result.statusCode = 200;
        result.message = "Posts loaded.";
        return result;
    }
    catch (const drogon::orm::DrogonDbException &)
    {
        result.statusCode = 500;
        result.message = "Database error while loading posts.";
        return result;
    }
    catch (const std::exception &)
    {
        result.statusCode = 500;
        result.message = "Server error while loading posts.";
        return result;
    }
}

BoardCreateResultDTO BoardService::createPost(
    std::uint64_t memberId,
    const CreateBoardPostRequestDTO &request)
{
    BoardCreateResultDTO result;

    // 세션이 없는 요청은 인증되지 않은 요청으로 처리한다.
    if (memberId == 0)
    {
        result.statusCode = 401;
        result.message = "Unauthorized.";
        return result;
    }

    // 검증 전에 사용자 입력 문자열을 정규화한다.
    const auto normalizedType = toLower(trim(request.type));
    const auto normalizedTitle = trim(request.title);
    const auto normalizedContent = trim(request.content);

    // 위치는 선택값이며, 공백 제거 후 값이 있을 때만 저장한다.
    std::optional<std::string> normalizedLocation;
    if (request.location.has_value())
    {
        const auto location = trim(*request.location);
        if (!location.empty())
        {
            normalizedLocation = location;
        }
    }

    // 게시글 유형은 지원되는 값이어야 한다.
    if (!isAllowedType(normalizedType))
    {
        result.statusCode = 400;
        result.message = "Type must be share or exchange.";
        return result;
    }

    // 제목은 필수다.
    if (normalizedTitle.empty())
    {
        result.statusCode = 400;
        result.message = "Title is required.";
        return result;
    }

    // 제목 길이 제한을 확인한다.
    if (normalizedTitle.size() > 100U)
    {
        result.statusCode = 400;
        result.message = "Title must be 100 characters or fewer.";
        return result;
    }

    // 내용은 필수다.
    if (normalizedContent.empty())
    {
        result.statusCode = 400;
        result.message = "Content is required.";
        return result;
    }

    // 선택값인 위치도 최대 길이 제한을 적용한다.
    if (normalizedLocation.has_value() && normalizedLocation->size() > 100U)
    {
        result.statusCode = 400;
        result.message = "Location must be 100 characters or fewer.";
        return result;
    }

    // DB 저장 형식에 맞는 요청 객체를 만든다.
    CreateBoardPostRequestDTO normalizedRequest;
    normalizedRequest.type = toDbType(normalizedType);
    normalizedRequest.title = normalizedTitle;
    normalizedRequest.content = normalizedContent;
    normalizedRequest.location = normalizedLocation;

    try
    {
        const auto created = mapper_.insertPost(memberId, normalizedRequest);
        if (!created.has_value())
        {
            result.statusCode = 500;
            result.message = "Failed to create post.";
            return result;
        }

        result.ok = true;
        result.statusCode = 201;
        result.message = "Post created.";
        result.post = created;
        return result;
    }
    catch (const drogon::orm::DrogonDbException &)
    {
        result.statusCode = 500;
        result.message = "Database error while creating post.";
        return result;
    }
    catch (const std::exception &)
    {
        result.statusCode = 500;
        result.message = "Server error while creating post.";
        return result;
    }
}

BoardDeleteResultDTO BoardService::deletePost(std::uint64_t memberId,
                                              std::uint64_t postId)
{
    BoardDeleteResultDTO result;

    // 세션이 없는 요청은 인증되지 않은 요청으로 처리한다.
    if (memberId == 0)
    {
        result.statusCode = 401;
        result.message = "Unauthorized.";
        return result;
    }

    // 삭제할 게시글 id는 필수다.
    if (postId == 0)
    {
        result.statusCode = 400;
        result.message = "Post id is required.";
        return result;
    }

    try
    {
        const auto post = mapper_.findById(postId);

        // 존재하지 않거나 이미 삭제된 게시글은 찾을 수 없는 것으로 처리한다.
        if (!post.has_value() || post->status == "deleted")
        {
            result.statusCode = 404;
            result.message = "Post not found.";
            return result;
        }

        // 작성자 본인만 게시글을 삭제할 수 있다.
        if (post->memberId != memberId)
        {
            result.statusCode = 403;
            result.message = "You can delete only your own post.";
            return result;
        }

        // 상태값을 변경해 소프트 삭제한다.
        mapper_.markPostDeleted(postId);

        result.ok = true;
        result.statusCode = 200;
        result.message = "Post deleted.";
        return result;
    }
    catch (const drogon::orm::DrogonDbException &)
    {
        result.statusCode = 500;
        result.message = "Database error while deleting post.";
        return result;
    }
    catch (const std::exception &)
    {
        result.statusCode = 500;
        result.message = "Server error while deleting post.";
        return result;
    }
}

std::string BoardService::trim(std::string value)
{
    // 문자열 앞뒤 공백을 제거하는 공용 유틸리티다.
    const auto notSpace = [](unsigned char ch) { return !std::isspace(ch); };

    value.erase(value.begin(),
                std::find_if(value.begin(), value.end(), notSpace));
    value.erase(
        std::find_if(value.rbegin(), value.rend(), notSpace).base(), value.end());

    return value;
}

std::string BoardService::toLower(std::string value)
{
    // 안정적인 비교를 위해 문자열을 소문자로 정규화한다.
    std::transform(value.begin(), value.end(), value.begin(), [](unsigned char ch) {
        return static_cast<char>(std::tolower(ch));
    });
    return value;
}

bool BoardService::isAllowedType(const std::string &value)
{
    // 현재 게시판은 나눔과 교환 게시글만 지원한다.
    return value == "share" || value == "exchange";
}

std::string BoardService::toDbType(const std::string &value)
{
    // DB 열거형 값은 대문자로 저장한다.
    if (value == "share")
    {
        return "SHARE";
    }
    return "EXCHANGE";
}

}  // 게시판 네임스페이스
