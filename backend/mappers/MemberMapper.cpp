#include "MemberMapper.h"

#include <drogon/drogon.h>

#include <atomic>

namespace
{

// DB 조회 결과 row를 서비스 내부에서 쓰는 MemberModel로 변환한다.
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

    // SQL에서 NULL을 빈 문자열로 바꿔 오므로 비어 있지 않을 때만 optional에 담는다.
    const auto lastLoginAt = row["last_login_at"].as<std::string>();
    if (!lastLoginAt.empty())
    {
        member.lastLoginAt = lastLoginAt;
    }

    return member;
}

}  // namespace

namespace auth
{

std::optional<MemberModel> MemberMapper::findByEmail(const std::string &email) const
{
    ensureMembersTable();

    // 로그인과 회원가입 중복 확인에서 사용하는 이메일 단건 조회 SQL이다.
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

    // 세션 검증 후 최신 회원 정보를 가져오기 위한 member_id 단건 조회 SQL이다.
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

    // passwordHash는 서비스에서 이미 만든 값만 받는다. mapper는 원문 비밀번호를 알지 않는다.
    const auto dbClient = drogon::app().getDbClient("default");
    dbClient->execSqlSync(
        "INSERT INTO members (email, password_hash, name) VALUES (?, ?, ?)",
        email,
        passwordHash,
        name);

    return findByEmail(email);
}

void MemberMapper::updateLastLoginAt(std::uint64_t memberId) const
{
    // 로그인 성공 또는 회원가입 직후 자동 로그인 시각을 기록한다.
    const auto dbClient = drogon::app().getDbClient("default");
    dbClient->execSqlSync(
        "UPDATE members SET last_login_at = NOW() WHERE member_id = ?",
        memberId);
}

void MemberMapper::updateLevelAndExp(std::uint64_t memberId,
                                     unsigned int level,
                                     unsigned int exp) const
{
    const auto dbClient = drogon::app().getDbClient("default");
    dbClient->execSqlSync(
        "UPDATE members SET level = ?, exp = ? WHERE member_id = ?",
        level,
        exp,
        memberId);
}

MemberDTO MemberMapper::toMemberDTO(const MemberModel &member) const
{
    // API 응답에 노출하면 안 되는 passwordHash는 여기서 제외한다.
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
    static std::atomic_bool tableReady{false};
    if (tableReady.load())
    {
        return;
    }

    // 로컬 실행에서 DB 스키마가 비어 있어도 로그인/회원가입을 바로 테스트할 수 있게 한다.
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

std::optional<MemberModel> MemberMapper::querySingleMember(
    const std::string &sql,
    const std::string &value) const
{
    // prepared parameter를 사용해 이메일 값을 SQL에 안전하게 바인딩한다.
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
    // member_id 기반 조회도 같은 단건 변환 흐름을 사용한다.
    const auto dbClient = drogon::app().getDbClient("default");
    const auto result = dbClient->execSqlSync(sql, memberId);
    if (result.empty())
    {
        return std::nullopt;
    }
    return rowToMemberModel(result[0]);
}

}  // namespace auth
