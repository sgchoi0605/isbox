/*
 * 파일 역할:
 * - BoardMapper의 SQL 실행/결과 매핑 구현체를 제공한다.
 * - 조건 필터(type/search) 조합에 맞는 COUNT/목록 조회 쿼리 분기를 수행한다.
 * - 로컬 개발에서 필요한 테이블 자동 생성 보장 로직을 포함한다.
 */
#include "board/mapper/BoardMapper.h"

#include <drogon/drogon.h>
#include <drogon/orm/Mapper.h>

#include <algorithm>
#include <atomic>
#include <cctype>
#include <optional>
#include <string>

#include "board/model/BoardPostModel.h"

namespace
{

// 대소문자 비교를 단순화하기 위한 소문자 복사 유틸.
// 원본 문자열은 보존하고 비교용 사본만 변환한다.
std::string toLowerCopy(std::string value)
{
    std::transform(value.begin(), value.end(), value.begin(), [](unsigned char ch) {
        return static_cast<char>(std::tolower(ch));
    });
    return value;
}

// LIKE 검색에 쓰는 `%keyword%` 패턴을 만든다.
// 완성된 패턴은 SQL 바인딩 인자로 전달되어 안전하게 사용된다.
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
    // 로컬 실행 시 필요한 테이블이 없으면 먼저 생성한다.
    // 운영에서는 마이그레이션을 사용하더라도 로컬 실행 편의를 위해 방어적으로 보장한다.
    ensureTablesForLocalDev();

    // 게시글 총 개수를 조건에 맞게 계산한다.
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

    // 필터 조합(타입/검색어)에 따라 SQL과 바인딩 인자를 맞춰 실행한다.
    // 각 분기에서 placeholder 개수와 바인딩 개수가 반드시 일치해야 한다.
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
        // COUNT 결과가 없으면 0건으로 본다.
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
        // limit 0은 조회 자체를 생략한다.
        return {};
    }

    // 작성자명까지 조인해 목록용 데이터를 조회한다.
    // 목록 화면에서 필요한 필드만 선택해 불필요한 전송량을 줄인다.
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

    // countPosts와 동일한 조건 조합으로 목록 조회 SQL을 분기한다.
    // 총 개수와 목록 결과 사이의 조건 불일치를 방지한다.
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
        // 각 DB 행을 API 전달용 DTO로 변환한다.
        posts.push_back(rowToBoardPostDTO(row));
    }
    return posts;
}

std::optional<BoardPostDTO> BoardMapper::findById(std::uint64_t postId) const
{
    ensureTablesForLocalDev();

    // 단건 상세 조회(작성자명 포함).
    // 삭제 권한 검증에서도 재사용되므로 작성자/상태 정보를 함께 조회한다.
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
        // 대상 게시글이 없으면 nullopt 반환.
        return std::nullopt;
    }

    return rowToBoardPostDTO(result[0]);
}

std::optional<BoardPostDTO> BoardMapper::insertPost(
    std::uint64_t memberId,
    const CreateBoardPostRequestDTO &request) const
{
    ensureTablesForLocalDev();

    // Drogon ORM 모델을 사용해 게시글을 저장한다.
    // 상태(status)는 생성 시점 기본값인 AVAILABLE로 저장한다.
    const auto dbClient = drogon::app().getDbClient("default");
    drogon::orm::Mapper<BoardPostModel> mapper(dbClient);

    BoardPostModel post(memberId,
                        request.type,
                        request.title,
                        request.content,
                        request.location.value_or(""),
                        "AVAILABLE");
    mapper.insert(post);

    // INSERT 후 방금 생성된 PK로 다시 조회해 최종 DTO를 반환한다.
    // DB 기본값/트리거 반영 결과를 포함한 최신 스냅샷을 전달하기 위함이다.
    return findById(post.postId());
}

bool BoardMapper::deletePost(std::uint64_t postId) const
{
    ensureTablesForLocalDev();

    // DELETE 영향 행 수가 1 이상이면 삭제 성공이다.
    // 이미 삭제됐거나 없는 postId는 0 rows가 반환된다.
    const auto dbClient = drogon::app().getDbClient("default");
    const auto result =
        dbClient->execSqlSync("DELETE FROM board_posts WHERE post_id = ?", postId);
    return result.affectedRows() > 0;
}

BoardPostDTO BoardMapper::rowToBoardPostDTO(const drogon::orm::Row &row)
{
    // DB 컬럼을 클라이언트 응답 DTO 구조에 맞게 매핑한다.
    // enum/nullable 컬럼은 API 계약(소문자 문자열/optional)에 맞게 정규화한다.
    BoardPostDTO dto;

    dto.postId = row["post_id"].as<std::uint64_t>();
    dto.memberId = row["member_id"].as<std::uint64_t>();
    dto.type = toClientType(row["post_type"].as<std::string>());
    dto.title = row["title"].as<std::string>();
    dto.content = row["content"].as<std::string>();

    const auto location = row["location"].as<std::string>();
    if (!location.empty())
    {
        // 빈 문자열 location은 optional 미설정으로 취급한다.
        dto.location = location;
    }

    dto.authorName = row["author_name"].as<std::string>();
    dto.status = toClientStatus(row["status"].as<std::string>());
    dto.createdAt = row["created_at"].as<std::string>();
    return dto;
}

std::string BoardMapper::toClientType(const std::string &dbType)
{
    // DB 대문자/혼용 값을 API 소문자 규약으로 맞춘다.
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
    // 상태값도 클라이언트 규약(소문자)으로 정규화한다.
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
    // 프로세스 수명 동안 최초 1회만 테이블 생성 SQL을 실행한다.
    // 반복 DDL 실행을 피하기 위해 atomic 플래그를 사용한다.
    static std::atomic_bool tablesReady{false};
    if (tablesReady.load())
    {
        return;
    }

    const auto dbClient = drogon::app().getDbClient("default");

    // 회원 테이블: 게시글 FK가 참조하므로 먼저 보장한다.
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

    // 게시글 테이블: 조회/정렬 성능을 위한 인덱스를 함께 생성한다.
    // 복합 인덱스(post_type, status, created_at)는 목록 필터 + 정렬 쿼리를 최적화한다.
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

    // 이후 호출에서는 생성 SQL을 생략한다.
    tablesReady.store(true);
}

}  // namespace board
