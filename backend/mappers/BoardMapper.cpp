#include "BoardMapper.h"

#include <drogon/drogon.h>
#include <drogon/orm/Mapper.h>

#include <algorithm>
#include <atomic>
#include <cctype>

#include "../models/BoardPostModel.h"

namespace
{

// enum 문자열 비교를 단순화하기 위해 소문자 사본을 만든다.
std::string toLowerCopy(std::string value)
{
    // DB enum이 대문자로 저장되어 있어도 API 응답은 소문자 규격으로 맞춘다.
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
    // 로컬 개발 환경에서 스키마가 없는 경우를 대비해 테이블을 보장한다.
    ensureTablesForLocalDev();

    // 목록 조회는 DELETED 상태를 제외하고, 작성자 이름을 함께 내려주기 위해 members를 조인한다.
    // 최신순(created_at DESC)으로 최대 50건만 반환한다.
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

    // 타입 필터가 있으면 WHERE 절에 post_type 조건을 추가한다.
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

    // 조회 결과를 DTO 벡터로 변환한다.
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
    // 로컬 개발 환경에서 스키마가 없는 경우를 대비해 테이블을 보장한다.
    ensureTablesForLocalDev();

    // 단건 조회도 목록과 동일한 컬럼 구성을 유지해 DTO 스키마를 통일한다.
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

    // 단건 조회이므로 첫 row를 DTO로 변환해 반환한다.
    return rowToBoardPostDTO(result[0]);
}

std::optional<BoardPostDTO> BoardMapper::insertPost(
    std::uint64_t memberId,
    const CreateBoardPostRequestDTO &request) const
{
    // 로컬 개발 환경에서 스키마가 없는 경우를 대비해 테이블을 보장한다.
    ensureTablesForLocalDev();

    // Mapper<BoardPostModel>을 사용해 insert를 수행한다.
    const auto dbClient = drogon::app().getDbClient("default");
    drogon::orm::Mapper<BoardPostModel> mapper(dbClient);

    // 신규 게시글 기본 상태는 AVAILABLE로 저장한다.
    BoardPostModel post(memberId,
                        request.type,
                        request.title,
                        request.content,
                        request.location.value_or(""),
                        "AVAILABLE");
    mapper.insert(post);

    // 삽입 후 post_id를 기준으로 재조회해 DTO를 반환한다.
    return findById(post.postId());
}

void BoardMapper::markPostDeleted(std::uint64_t postId) const
{
    // 로컬 개발 환경에서 스키마가 없는 경우를 대비해 테이블을 보장한다.
    ensureTablesForLocalDev();

    // 물리 삭제 대신 상태값만 DELETED로 변경해 이력/복구 가능성을 유지한다.
    const auto dbClient = drogon::app().getDbClient("default");
    dbClient->execSqlSync(
        "UPDATE board_posts SET status = 'DELETED' WHERE post_id = ?",
        postId);
}

BoardPostDTO BoardMapper::rowToBoardPostDTO(const drogon::orm::Row &row)
{
    // DB 컬럼명을 API 응답 DTO 필드명으로 매핑한다.
    BoardPostDTO dto;

    // 기본 게시글 정보
    dto.postId = row["post_id"].as<std::uint64_t>();
    dto.memberId = row["member_id"].as<std::uint64_t>();
    dto.type = toClientType(row["post_type"].as<std::string>());
    dto.title = row["title"].as<std::string>();
    dto.content = row["content"].as<std::string>();

    // location은 빈 문자열이면 값 없음(optional 미설정)으로 처리한다.
    const auto location = row["location"].as<std::string>();
    if (!location.empty())
    {
        dto.location = location;
    }

    // 작성자/상태/작성시각 정보
    dto.authorName = row["author_name"].as<std::string>();
    dto.status = toClientStatus(row["status"].as<std::string>());
    dto.createdAt = row["created_at"].as<std::string>();
    return dto;
}

std::string BoardMapper::toClientType(const std::string &dbType)
{
    // DB enum(SHARE/EXCHANGE)을 API 규격 소문자 문자열로 정규화한다.
    const auto lowered = toLowerCopy(dbType);
    if (lowered == "share")
    {
        return "share";
    }
    if (lowered == "exchange")
    {
        return "exchange";
    }

    // 예상 외 값이 와도 정보 손실 없이 소문자 원문을 반환한다.
    return lowered;
}

std::string BoardMapper::toClientStatus(const std::string &dbStatus)
{
    // DB enum(AVAILABLE/CLOSED/DELETED)을 API 규격 소문자 문자열로 정규화한다.
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

    // 예상 외 값이 와도 정보 손실 없이 소문자 원문을 반환한다.
    return lowered;
}

void BoardMapper::ensureTablesForLocalDev() const
{
    // 프로세스 생애주기 동안 DDL을 1회만 실행하기 위한 플래그
    static std::atomic_bool tablesReady{false};
    if (tablesReady.load())
    {
        return;
    }

    const auto dbClient = drogon::app().getDbClient("default");

    // board_posts의 FK 대상이 되는 members 테이블을 먼저 보장한다.
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

    // 로컬 실행에 필요한 board_posts 최소 스키마를 자동 생성한다.
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

    // 이후 호출에서는 DDL을 건너뛴다.
    tablesReady.store(true);
}

}  // namespace board
