#include "BoardService.h"

#include <drogon/orm/Exception.h>

#include <algorithm>
#include <cctype>

namespace board
{

BoardListResultDTO BoardService::getPosts(const BoardListRequestDTO &request)
{
    BoardListResultDTO result;

    if (request.page == 0)
    {
        result.statusCode = 400;
        result.message = "Invalid page parameter.";
        return result;
    }

    if (request.size == 0 || request.size > 50)
    {
        result.statusCode = 400;
        result.message = "Invalid size parameter.";
        return result;
    }

    std::optional<std::string> dbTypeFilter;
    const auto normalizedType = toLower(trim(request.type));
    if (!normalizedType.empty() && normalizedType != "all")
    {
        if (!isAllowedType(normalizedType))
        {
            result.statusCode = 400;
            result.message = "Invalid post type filter.";
            return result;
        }
        dbTypeFilter = toDbType(normalizedType);
    }

    std::optional<std::string> searchKeyword;
    const auto normalizedSearch = trim(request.search);
    if (!normalizedSearch.empty())
    {
        searchKeyword = normalizedSearch;
    }

    result.pagination.page = request.page;
    result.pagination.size = request.size;

    try
    {
        const auto totalItems = mapper_.countPosts(dbTypeFilter, searchKeyword);
        result.pagination.totalItems = totalItems;

        const auto totalPages =
            totalItems == 0
                ? 0U
                : static_cast<std::uint32_t>(
                      (totalItems + static_cast<std::uint64_t>(request.size) - 1ULL) /
                      static_cast<std::uint64_t>(request.size));
        result.pagination.totalPages = totalPages;

        std::uint32_t resolvedPage = request.page;
        if (totalPages == 0)
        {
            resolvedPage = 1;
        }
        else if (resolvedPage > totalPages)
        {
            resolvedPage = totalPages;
        }

        result.pagination.page = resolvedPage;
        result.pagination.hasPrev = totalPages > 0 && resolvedPage > 1;
        result.pagination.hasNext = totalPages > 0 && resolvedPage < totalPages;

        const auto offset = totalPages == 0
                                ? 0ULL
                                : static_cast<std::uint64_t>(resolvedPage - 1U) *
                                      static_cast<std::uint64_t>(request.size);

        if (totalItems > 0)
        {
            result.posts =
                mapper_.findPosts(dbTypeFilter, searchKeyword, request.size, offset);
        }

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
    const auto notSpace = [](unsigned char ch) { return !std::isspace(ch); };

    value.erase(value.begin(),
                std::find_if(value.begin(), value.end(), notSpace));
    value.erase(
        std::find_if(value.rbegin(), value.rend(), notSpace).base(), value.end());

    return value;
}

std::string BoardService::toLower(std::string value)
{
    std::transform(value.begin(), value.end(), value.begin(), [](unsigned char ch) {
        return static_cast<char>(std::tolower(ch));
    });
    return value;
}

bool BoardService::isAllowedType(const std::string &value)
{
    return value == "share" || value == "exchange";
}

std::string BoardService::toDbType(const std::string &value)
{
    if (value == "share")
    {
        return "SHARE";
    }
    return "EXCHANGE";
}

}  // namespace board
