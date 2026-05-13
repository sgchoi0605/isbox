#include "BoardMapper.h"

#include <drogon/drogon.h>
#include <drogon/orm/Mapper.h>

#include <algorithm>
#include <atomic>
#include <cctype>
#include <optional>
#include <string>

#include "../models/BoardPostModel.h"

namespace
{

std::string toLowerCopy(std::string value)
{
    std::transform(value.begin(), value.end(), value.begin(), [](unsigned char ch) {
        return static_cast<char>(std::tolower(ch));
    });
    return value;
}

std::string buildLikePattern(const std::string &keyword)
{
    return "%" + keyword + "%";
}

}  // namespace

namespace board
{

std::uint64_t BoardMapper::countPosts(
    const std::optional<std::string> &dbTypeFilter,
    const std::optional<std::string> &titleSearchKeyword) const
{
    ensureTablesForLocalDev();

    const auto dbClient = drogon::app().getDbClient("default");
    static const std::string baseSql =
        "SELECT COUNT(*) AS total_count "
        "FROM board_posts p "
        "WHERE p.status <> 'DELETED' ";

    const auto hasTypeFilter = dbTypeFilter.has_value();
    const auto hasSearchKeyword = titleSearchKeyword.has_value();
    const auto likePattern = hasSearchKeyword
                                 ? std::optional<std::string>(
                                       buildLikePattern(*titleSearchKeyword))
                                 : std::nullopt;

    drogon::orm::Result result;
    if (hasTypeFilter && hasSearchKeyword)
    {
        result = dbClient->execSqlSync(
            baseSql + "AND p.post_type = ? AND LOWER(p.title) LIKE LOWER(?)",
            *dbTypeFilter,
            *likePattern);
    }
    else if (hasTypeFilter)
    {
        result = dbClient->execSqlSync(baseSql + "AND p.post_type = ?", *dbTypeFilter);
    }
    else if (hasSearchKeyword)
    {
        result = dbClient->execSqlSync(baseSql + "AND LOWER(p.title) LIKE LOWER(?)",
                                       *likePattern);
    }
    else
    {
        result = dbClient->execSqlSync(baseSql);
    }

    if (result.empty())
    {
        return 0;
    }

    return result[0]["total_count"].as<std::uint64_t>();
}

std::vector<BoardPostDTO> BoardMapper::findPosts(
    const std::optional<std::string> &dbTypeFilter,
    const std::optional<std::string> &titleSearchKeyword,
    std::uint32_t limit,
    std::uint64_t offset) const
{
    ensureTablesForLocalDev();

    if (limit == 0)
    {
        return {};
    }

    const auto dbClient = drogon::app().getDbClient("default");
    static const std::string baseSql =
        "SELECT "
        "p.post_id, p.member_id, p.post_type, p.title, p.content, "
        "COALESCE(p.location, '') AS location, p.status, "
        "COALESCE(DATE_FORMAT(p.created_at, '%Y-%m-%d %H:%i:%s'), '') AS created_at, "
        "m.name AS author_name "
        "FROM board_posts p "
        "JOIN members m ON m.member_id = p.member_id "
        "WHERE p.status <> 'DELETED' ";

    static const std::string orderSql = "ORDER BY p.created_at DESC LIMIT ? OFFSET ?";

    const auto hasTypeFilter = dbTypeFilter.has_value();
    const auto hasSearchKeyword = titleSearchKeyword.has_value();
    const auto likePattern = hasSearchKeyword
                                 ? std::optional<std::string>(
                                       buildLikePattern(*titleSearchKeyword))
                                 : std::nullopt;

    drogon::orm::Result result;
    if (hasTypeFilter && hasSearchKeyword)
    {
        result = dbClient->execSqlSync(
            baseSql + "AND p.post_type = ? AND LOWER(p.title) LIKE LOWER(?) " + orderSql,
            *dbTypeFilter,
            *likePattern,
            limit,
            offset);
    }
    else if (hasTypeFilter)
    {
        result = dbClient->execSqlSync(baseSql + "AND p.post_type = ? " + orderSql,
                                       *dbTypeFilter,
                                       limit,
                                       offset);
    }
    else if (hasSearchKeyword)
    {
        result = dbClient->execSqlSync(baseSql + "AND LOWER(p.title) LIKE LOWER(?) " + orderSql,
                                       *likePattern,
                                       limit,
                                       offset);
    }
    else
    {
        result = dbClient->execSqlSync(baseSql + orderSql, limit, offset);
    }

    std::vector<BoardPostDTO> posts;
    posts.reserve(result.size());
    for (const auto &row : result)
    {
        posts.push_back(rowToBoardPostDTO(row));
    }
    return posts;
}

std::optional<BoardPostDTO> BoardMapper::findById(std::uint64_t postId) const
{
    ensureTablesForLocalDev();

    const auto dbClient = drogon::app().getDbClient("default");
    static const std::string sql =
        "SELECT "
        "p.post_id, p.member_id, p.post_type, p.title, p.content, "
        "COALESCE(p.location, '') AS location, p.status, "
        "COALESCE(DATE_FORMAT(p.created_at, '%Y-%m-%d %H:%i:%s'), '') AS created_at, "
        "m.name AS author_name "
        "FROM board_posts p "
        "JOIN members m ON m.member_id = p.member_id "
        "WHERE p.post_id = ? LIMIT 1";

    const auto result = dbClient->execSqlSync(sql, postId);
    if (result.empty())
    {
        return std::nullopt;
    }

    return rowToBoardPostDTO(result[0]);
}

std::optional<BoardPostDTO> BoardMapper::insertPost(
    std::uint64_t memberId,
    const CreateBoardPostRequestDTO &request) const
{
    ensureTablesForLocalDev();

    const auto dbClient = drogon::app().getDbClient("default");
    drogon::orm::Mapper<BoardPostModel> mapper(dbClient);

    BoardPostModel post(memberId,
                        request.type,
                        request.title,
                        request.content,
                        request.location.value_or(""),
                        "AVAILABLE");
    mapper.insert(post);

    return findById(post.postId());
}

void BoardMapper::markPostDeleted(std::uint64_t postId) const
{
    ensureTablesForLocalDev();

    const auto dbClient = drogon::app().getDbClient("default");
    dbClient->execSqlSync(
        "UPDATE board_posts SET status = 'DELETED' WHERE post_id = ?",
        postId);
}

BoardPostDTO BoardMapper::rowToBoardPostDTO(const drogon::orm::Row &row)
{
    BoardPostDTO dto;

    dto.postId = row["post_id"].as<std::uint64_t>();
    dto.memberId = row["member_id"].as<std::uint64_t>();
    dto.type = toClientType(row["post_type"].as<std::string>());
    dto.title = row["title"].as<std::string>();
    dto.content = row["content"].as<std::string>();

    const auto location = row["location"].as<std::string>();
    if (!location.empty())
    {
        dto.location = location;
    }

    dto.authorName = row["author_name"].as<std::string>();
    dto.status = toClientStatus(row["status"].as<std::string>());
    dto.createdAt = row["created_at"].as<std::string>();
    return dto;
}

std::string BoardMapper::toClientType(const std::string &dbType)
{
    const auto lowered = toLowerCopy(dbType);
    if (lowered == "share")
    {
        return "share";
    }
    if (lowered == "exchange")
    {
        return "exchange";
    }

    return lowered;
}

std::string BoardMapper::toClientStatus(const std::string &dbStatus)
{
    const auto lowered = toLowerCopy(dbStatus);
    if (lowered == "available")
    {
        return "available";
    }
    if (lowered == "closed")
    {
        return "closed";
    }
    if (lowered == "deleted")
    {
        return "deleted";
    }

    return lowered;
}

void BoardMapper::ensureTablesForLocalDev() const
{
    static std::atomic_bool tablesReady{false};
    if (tablesReady.load())
    {
        return;
    }

    const auto dbClient = drogon::app().getDbClient("default");

    dbClient->execSqlSync(
        "CREATE TABLE IF NOT EXISTS members ("
        "member_id BIGINT UNSIGNED NOT NULL AUTO_INCREMENT,"
        "email VARCHAR(255) NOT NULL,"
        "password_hash VARCHAR(255) NOT NULL,"
        "name VARCHAR(100) NOT NULL,"
        "role VARCHAR(20) NOT NULL DEFAULT 'USER',"
        "status VARCHAR(20) NOT NULL DEFAULT 'ACTIVE',"
        "level INT UNSIGNED NOT NULL DEFAULT 1,"
        "exp INT UNSIGNED NOT NULL DEFAULT 0,"
        "last_login_at DATETIME NULL,"
        "created_at DATETIME NOT NULL DEFAULT CURRENT_TIMESTAMP,"
        "updated_at DATETIME NOT NULL DEFAULT CURRENT_TIMESTAMP ON UPDATE "
        "CURRENT_TIMESTAMP,"
        "PRIMARY KEY (member_id),"
        "UNIQUE KEY uq_members_email (email),"
        "KEY idx_members_status (status)"
        ") ENGINE=InnoDB DEFAULT CHARSET=utf8mb4");

    dbClient->execSqlSync(
        "CREATE TABLE IF NOT EXISTS board_posts ("
        "post_id BIGINT UNSIGNED NOT NULL AUTO_INCREMENT,"
        "member_id BIGINT UNSIGNED NOT NULL,"
        "post_type ENUM('SHARE', 'EXCHANGE') NOT NULL,"
        "title VARCHAR(100) NOT NULL,"
        "content TEXT NOT NULL,"
        "location VARCHAR(100) NULL,"
        "status ENUM('AVAILABLE', 'CLOSED', 'DELETED') NOT NULL DEFAULT 'AVAILABLE',"
        "view_count INT UNSIGNED NOT NULL DEFAULT 0,"
        "created_at DATETIME NOT NULL DEFAULT CURRENT_TIMESTAMP,"
        "updated_at DATETIME NOT NULL DEFAULT CURRENT_TIMESTAMP ON UPDATE CURRENT_TIMESTAMP,"
        "PRIMARY KEY (post_id),"
        "KEY idx_board_posts_member_id (member_id),"
        "KEY idx_board_posts_type_status_created (post_type, status, created_at),"
        "KEY idx_board_posts_created_at (created_at),"
        "CONSTRAINT fk_board_posts_member FOREIGN KEY (member_id) "
        "REFERENCES members(member_id) ON DELETE CASCADE"
        ") ENGINE=InnoDB DEFAULT CHARSET=utf8mb4");

    tablesReady.store(true);
}

}  // namespace board
