#include "BoardMapper.h"

#include <drogon/drogon.h>
#include <drogon/orm/Mapper.h>

#include <algorithm>
#include <atomic>
#include <cctype>

#include "../models/BoardPostModel.h"

namespace
{

std::string toLowerCopy(std::string value)
{
    // DB enum 값이 대문자로 오더라도 응답 DTO는 소문자 문자열을 사용한다.
    std::transform(value.begin(), value.end(), value.begin(), [](unsigned char ch) {
        return static_cast<char>(std::tolower(ch));
    });
    return value;
}

}  // namespace

namespace board
{

std::vector<BoardPostDTO> BoardMapper::findPosts(
    const std::optional<std::string> &dbTypeFilter) const
{
    // 목록 화면은 삭제된 글을 제외하고, 작성자 이름을 함께 보여주기 위해 members와 조인한다.
    // 정렬 기준은 created_at DESC이며 처음 50개만 반환한다.
    ensureTablesForLocalDev();

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

    static const std::string orderSql = "ORDER BY p.created_at DESC LIMIT 50";

    drogon::orm::Result result;
    if (dbTypeFilter.has_value())
    {
        result = dbClient->execSqlSync(baseSql + "AND p.post_type = ? " + orderSql,
                                       *dbTypeFilter);
    }
    else
    {
        result = dbClient->execSqlSync(baseSql + orderSql);
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
    // 단건 조회도 목록과 같은 DTO 모양을 유지하기 위해 동일한 조인 컬럼을 선택한다.
    // 결과가 없으면 서비스 계층에서 404나 권한 검증 실패로 처리할 수 있게 nullopt를 반환한다.
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
    // 삽입은 Drogon Mapper가 담당하고, 응답에는 authorName/createdAt까지 필요하므로 다시 조회한다.
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
    // 게시글 복구나 감사 가능성을 남기기 위해 물리 삭제 대신 상태값만 변경한다.
    ensureTablesForLocalDev();

    const auto dbClient = drogon::app().getDbClient("default");
    dbClient->execSqlSync(
        "UPDATE board_posts SET status = 'DELETED' WHERE post_id = ?",
        postId);
}

BoardPostDTO BoardMapper::rowToBoardPostDTO(const drogon::orm::Row &row)
{
    // DB 컬럼명과 API 응답 필드명이 일부 다르므로 여기에서 한 번에 변환한다.
    // 빈 location은 optional 값이 없는 것으로 취급해 응답 JSON에서 생략할 수 있게 한다.
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
    // DB enum은 SHARE/EXCHANGE이고 프론트엔드는 share/exchange를 사용한다.
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
    // DB enum은 AVAILABLE/CLOSED/DELETED이고 프론트엔드는 소문자 상태값을 사용한다.
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
    // 마이그레이션 없이 로컬 실행이 가능하도록 최소 스키마를 준비한다.
    // static 플래그로 프로세스당 한 번만 실행해 매 요청마다 DDL을 호출하지 않게 한다.
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
