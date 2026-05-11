#include "BoardService.h"

#include <drogon/orm/Exception.h>

#include <algorithm>
#include <cctype>

namespace board
{

BoardListResultDTO BoardService::getPosts(
    const std::optional<std::string> &typeFilter)
{
    // 클라이언트의 필터값을 서비스에서 표준화한 뒤 DB가 이해하는 enum 문자열로 넘긴다.
    // 잘못된 필터는 DB 조회 전에 400 응답으로 끝낸다.
    BoardListResultDTO result;

    std::optional<std::string> dbTypeFilter;
    if (typeFilter.has_value())
    {
        const auto normalized = toLower(trim(*typeFilter));
        if (!normalized.empty() && normalized != "all")
        {
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
    // 저장 전에 사용자 입력을 정규화하고 검증한다.
    // 이 함수가 통과시킨 값만 BoardMapper로 내려가므로 DB 계층은 저장에 집중한다.
    BoardCreateResultDTO result;

    if (memberId == 0)
    {
        result.statusCode = 401;
        result.message = "Unauthorized.";
        return result;
    }

    const auto normalizedType = toLower(trim(request.type));
    const auto normalizedTitle = trim(request.title);
    const auto normalizedContent = trim(request.content);

    std::optional<std::string> normalizedLocation;
    if (request.location.has_value())
    {
        const auto location = trim(*request.location);
        if (!location.empty())
        {
            normalizedLocation = location;
        }
    }

    if (!isAllowedType(normalizedType))
    {
        result.statusCode = 400;
        result.message = "Type must be share or exchange.";
        return result;
    }

    if (normalizedTitle.empty())
    {
        result.statusCode = 400;
        result.message = "Title is required.";
        return result;
    }

    if (normalizedTitle.size() > 100U)
    {
        result.statusCode = 400;
        result.message = "Title must be 100 characters or fewer.";
        return result;
    }

    if (normalizedContent.empty())
    {
        result.statusCode = 400;
        result.message = "Content is required.";
        return result;
    }

    if (normalizedLocation.has_value() && normalizedLocation->size() > 100U)
    {
        result.statusCode = 400;
        result.message = "Location must be 100 characters or fewer.";
        return result;
    }

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
    // 삭제 요청은 작성자 본인만 허용한다.
    // 실제 행은 지우지 않고 status를 DELETED로 바꿔 목록 조회에서 제외한다.
    BoardDeleteResultDTO result;

    if (memberId == 0)
    {
        result.statusCode = 401;
        result.message = "Unauthorized.";
        return result;
    }

    if (postId == 0)
    {
        result.statusCode = 400;
        result.message = "Post id is required.";
        return result;
    }

    try
    {
        const auto post = mapper_.findById(postId);
        if (!post.has_value() || post->status == "deleted")
        {
            result.statusCode = 404;
            result.message = "Post not found.";
            return result;
        }

        if (post->memberId != memberId)
        {
            result.statusCode = 403;
            result.message = "You can delete only your own post.";
            return result;
        }

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
    // 제목/본문/위치 검증에서 공백만 있는 입력을 빈 문자열로 판정하기 위한 유틸리티다.
    const auto notSpace = [](unsigned char ch) { return !std::isspace(ch); };

    value.erase(value.begin(),
                std::find_if(value.begin(), value.end(), notSpace));
    value.erase(
        std::find_if(value.rbegin(), value.rend(), notSpace).base(), value.end());

    return value;
}

std::string BoardService::toLower(std::string value)
{
    // share, SHARE, Share처럼 섞여 들어온 입력을 같은 값으로 다룬다.
    std::transform(value.begin(), value.end(), value.begin(), [](unsigned char ch) {
        return static_cast<char>(std::tolower(ch));
    });
    return value;
}

bool BoardService::isAllowedType(const std::string &value)
{
    // 현재 게시판은 나눔(share)과 교환(exchange) 두 유형만 허용한다.
    return value == "share" || value == "exchange";
}

std::string BoardService::toDbType(const std::string &value)
{
    // DB 컬럼은 ENUM('SHARE', 'EXCHANGE')이므로 저장 직전에 대문자 enum 값으로 변환한다.
    if (value == "share")
    {
        return "SHARE";
    }
    return "EXCHANGE";
}

}  // namespace board
