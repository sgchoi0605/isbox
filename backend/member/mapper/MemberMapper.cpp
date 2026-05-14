/*
 * 파일 개요: members/member_food_mbti 테이블에 대한 SQL 실행 로직을 담은 매퍼 구현 파일이다.
 * 주요 역할: 회원/MBTI 조회 및 갱신 쿼리 실행, DB row와 도메인 모델 변환, 테이블 보장.
 */
#include "member/mapper/MemberMapper.h"

#include <drogon/drogon.h>

#include <atomic>

namespace
{

// DB 조회 결과 row를 내부 MemberModel로 변환한다.
auth::MemberModel rowToMemberModel(const drogon::orm::Row &row)
{
    auth::MemberModel member;

    member.memberId = row["member_id"].as<std::uint64_t>();
    member.email = row["email"].as<std::string>();
    member.passwordHash = row["password_hash"].as<std::string>();
    member.name = row["name"].as<std::string>();
    member.role = row["role"].as<std::string>();
    member.status = row["status"].as<std::string>();
    member.level = row["level"].as<unsigned int>();
    member.exp = row["exp"].as<unsigned int>();

    const auto lastLoginAt = row["last_login_at"].as<std::string>();
    if (!lastLoginAt.empty())
    {
        member.lastLoginAt = lastLoginAt;
    }

    return member;
}

// DB 조회 결과 row를 FoodMbtiModel로 변환한다.
auth::FoodMbtiModel rowToFoodMbtiModel(const drogon::orm::Row &row)
{
    auth::FoodMbtiModel model;
    model.memberId = row["member_id"].as<std::uint64_t>();
    model.type = row["mbti_type"].as<std::string>();
    model.title = row["title"].as<std::string>();
    model.description = row["description"].as<std::string>();
    model.traitsJson = row["traits_json"].as<std::string>();
    model.recommendedFoodsJson = row["recommended_foods_json"].as<std::string>();
    model.completedAt = row["completed_at"].as<std::string>();
    model.updatedAt = row["updated_at"].as<std::string>();
    return model;
}

}  // namespace

namespace auth
{

std::optional<MemberModel> MemberMapper::findByEmail(const std::string &email) const
{
    // 로컬 개발에서 테이블이 없는 경우를 대비해 선보장한다.
    ensureMembersTable();

    static const std::string sql =
        "SELECT "
        "member_id, email, password_hash, name, role, status, level, exp, "
        "COALESCE(DATE_FORMAT(last_login_at, '%Y-%m-%d %H:%i:%s'), '') AS "
        "last_login_at "
        "FROM members WHERE email = ? LIMIT 1";

    return querySingleMember(sql, email);
}

std::optional<MemberModel> MemberMapper::findById(std::uint64_t memberId) const
{
    ensureMembersTable();

    static const std::string sql =
        "SELECT "
        "member_id, email, password_hash, name, role, status, level, exp, "
        "COALESCE(DATE_FORMAT(last_login_at, '%Y-%m-%d %H:%i:%s'), '') AS "
        "last_login_at "
        "FROM members WHERE member_id = ? LIMIT 1";

    return querySingleMemberById(sql, memberId);
}

std::optional<MemberModel> MemberMapper::createMember(const std::string &email,
                                                      const std::string &passwordHash,
                                                      const std::string &name) const
{
    ensureMembersTable();

    const auto dbClient = drogon::app().getDbClient("default");
    dbClient->execSqlSync(
        "INSERT INTO members (email, password_hash, name) VALUES (?, ?, ?)",
        email,
        passwordHash,
        name);

    // INSERT 직후 최신 row를 다시 조회해 반환한다.
    return findByEmail(email);
}

void MemberMapper::updateLastLoginAt(std::uint64_t memberId) const
{
    // 로그인 성공 시점을 기록한다.
    const auto dbClient = drogon::app().getDbClient("default");
    dbClient->execSqlSync(
        "UPDATE members SET last_login_at = NOW() WHERE member_id = ?",
        memberId);
}

void MemberMapper::updateProfile(std::uint64_t memberId,
                                 const std::string &email,
                                 const std::string &name) const
{
    // 프로필 편집 결과를 DB에 반영한다.
    const auto dbClient = drogon::app().getDbClient("default");
    dbClient->execSqlSync(
        "UPDATE members SET email = ?, name = ? WHERE member_id = ?",
        email,
        name,
        memberId);
}

void MemberMapper::updatePasswordHash(std::uint64_t memberId,
                                      const std::string &passwordHash) const
{
    // 비밀번호 해시만 갱신한다.
    const auto dbClient = drogon::app().getDbClient("default");
    dbClient->execSqlSync(
        "UPDATE members SET password_hash = ? WHERE member_id = ?",
        passwordHash,
        memberId);
}

void MemberMapper::updateLevelAndExp(std::uint64_t memberId,
                                     unsigned int level,
                                     unsigned int exp) const
{
    // 계산된 레벨/경험치를 원자적으로 갱신한다.
    const auto dbClient = drogon::app().getDbClient("default");
    dbClient->execSqlSync(
        "UPDATE members SET level = ?, exp = ? WHERE member_id = ?",
        level,
        exp,
        memberId);
}

void MemberMapper::upsertFoodMbti(
    std::uint64_t memberId,
    const std::string &type,
    const std::string &title,
    const std::string &description,
    const std::string &traitsJson,
    const std::string &recommendedFoodsJson,
    const std::optional<std::string> &completedAt) const
{
    // member_id를 키로 INSERT 또는 UPDATE를 수행한다.
    ensureMemberFoodMbtiTable();

    const auto dbClient = drogon::app().getDbClient("default");
    const auto completedAtValue = completedAt.has_value() ? *completedAt : "";

    dbClient->execSqlSync(
        "INSERT INTO member_food_mbti ("
        "member_id, mbti_type, title, description, traits_json, recommended_foods_json, completed_at"
        ") VALUES (?, ?, ?, NULLIF(?, ''), ?, ?, IFNULL(NULLIF(?, ''), NOW())) "
        "ON DUPLICATE KEY UPDATE "
        "mbti_type = VALUES(mbti_type), "
        "title = VALUES(title), "
        "description = VALUES(description), "
        "traits_json = VALUES(traits_json), "
        "recommended_foods_json = VALUES(recommended_foods_json), "
        "completed_at = VALUES(completed_at)",
        memberId,
        type,
        title,
        description,
        traitsJson,
        recommendedFoodsJson,
        completedAtValue);
}

std::optional<FoodMbtiModel> MemberMapper::findFoodMbtiByMemberId(
    std::uint64_t memberId) const
{
    // 회원 1명에 대한 MBTI 단건을 조회한다.
    ensureMemberFoodMbtiTable();

    const auto dbClient = drogon::app().getDbClient("default");
    static const std::string sql =
        "SELECT "
        "member_id, mbti_type, title, "
        "COALESCE(description, '') AS description, "
        "COALESCE(traits_json, '[]') AS traits_json, "
        "COALESCE(recommended_foods_json, '[]') AS recommended_foods_json, "
        "COALESCE(DATE_FORMAT(completed_at, '%Y-%m-%d %H:%i:%s'), '') AS completed_at, "
        "COALESCE(DATE_FORMAT(updated_at, '%Y-%m-%d %H:%i:%s'), '') AS updated_at "
        "FROM member_food_mbti "
        "WHERE member_id = ? "
        "LIMIT 1";

    const auto result = dbClient->execSqlSync(sql, memberId);
    if (result.empty())
    {
        return std::nullopt;
    }

    return rowToFoodMbtiModel(result[0]);
}

MemberDTO MemberMapper::toMemberDTO(const MemberModel &member) const
{
    // passwordHash는 응답에서 제외하고 안전한 필드만 매핑한다.
    MemberDTO dto;
    dto.memberId = member.memberId;
    dto.email = member.email;
    dto.name = member.name;
    dto.role = member.role;
    dto.status = member.status;
    dto.level = member.level;
    dto.exp = member.exp;
    dto.lastLoginAt = member.lastLoginAt;
    return dto;
}

void MemberMapper::ensureMembersTable() const
{
    // 프로세스 생명주기 동안 1회만 DDL을 수행한다.
    static std::atomic_bool tableReady{false};
    if (tableReady.load())
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

    tableReady.store(true);
}

void MemberMapper::ensureMemberFoodMbtiTable() const
{
    // 외래키 대상 보장을 위해 members 테이블부터 준비한다.
    ensureMembersTable();

    static std::atomic_bool tableReady{false};
    if (tableReady.load())
    {
        return;
    }

    const auto dbClient = drogon::app().getDbClient("default");
    dbClient->execSqlSync(
        "CREATE TABLE IF NOT EXISTS member_food_mbti ("
        "member_id BIGINT UNSIGNED NOT NULL,"
        "mbti_type VARCHAR(20) NOT NULL,"
        "title VARCHAR(100) NOT NULL,"
        "description TEXT NULL,"
        "traits_json TEXT NULL,"
        "recommended_foods_json TEXT NULL,"
        "completed_at DATETIME NOT NULL DEFAULT CURRENT_TIMESTAMP,"
        "updated_at DATETIME NOT NULL DEFAULT CURRENT_TIMESTAMP ON UPDATE CURRENT_TIMESTAMP,"
        "PRIMARY KEY (member_id),"
        "CONSTRAINT fk_member_food_mbti_member FOREIGN KEY (member_id) "
        "REFERENCES members(member_id) ON DELETE CASCADE"
        ") ENGINE=InnoDB DEFAULT CHARSET=utf8mb4");

    tableReady.store(true);
}

std::optional<MemberModel> MemberMapper::querySingleMember(
    const std::string &sql,
    const std::string &value) const
{
    // 문자열 파라미터 기반 prepared query를 실행한다.
    const auto dbClient = drogon::app().getDbClient("default");
    const auto result = dbClient->execSqlSync(sql, value);
    if (result.empty())
    {
        return std::nullopt;
    }
    return rowToMemberModel(result[0]);
}

std::optional<MemberModel> MemberMapper::querySingleMemberById(
    const std::string &sql,
    std::uint64_t memberId) const
{
    // 숫자 ID 파라미터 기반 prepared query를 실행한다.
    const auto dbClient = drogon::app().getDbClient("default");
    const auto result = dbClient->execSqlSync(sql, memberId);
    if (result.empty())
    {
        return std::nullopt;
    }
    return rowToMemberModel(result[0]);
}

}  // namespace auth

