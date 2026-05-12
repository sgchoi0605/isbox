#include "MemberMapper.h"

#include <drogon/drogon.h>

#include <atomic>

namespace
{

// DB 조회 결과의 1개 row를 서비스 레이어에서 사용하는 MemberModel로 변환한다.
auth::MemberModel rowToMemberModel(const drogon::orm::Row &row)
{
    auth::MemberModel member;

    // 기본 식별/계정 정보 매핑
    member.memberId = row["member_id"].as<std::uint64_t>();
    member.email = row["email"].as<std::string>();
    member.passwordHash = row["password_hash"].as<std::string>();
    member.name = row["name"].as<std::string>();

    // 권한/상태/성장 정보 매핑
    member.role = row["role"].as<std::string>();
    member.status = row["status"].as<std::string>();
    member.level = row["level"].as<unsigned int>();
    member.exp = row["exp"].as<unsigned int>();

    // SQL에서 NULL을 빈 문자열로 치환해 가져오므로,
    // 실제 값이 있을 때만 optional 필드를 채운다.
    const auto lastLoginAt = row["last_login_at"].as<std::string>();
    if (!lastLoginAt.empty())
    {
        member.lastLoginAt = lastLoginAt;
    }

    return member;
}

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
    // 로컬 개발 환경에서 스키마가 없는 경우를 대비해 테이블을 보장한다.
    ensureMembersTable();

    // 로그인/중복 체크에 사용하는 단건 조회 SQL
    static const std::string sql =
        "SELECT "
        "member_id, email, password_hash, name, role, status, level, exp, "
        "COALESCE(DATE_FORMAT(last_login_at, '%Y-%m-%d %H:%i:%s'), '') AS "
        "last_login_at "
        "FROM members WHERE email = ? LIMIT 1";

    // 이메일을 prepared parameter로 바인딩해 안전하게 조회한다.
    return querySingleMember(sql, email);
}

std::optional<MemberModel> MemberMapper::findById(std::uint64_t memberId) const
{
    // 로컬 개발 환경에서 스키마가 없는 경우를 대비해 테이블을 보장한다.
    ensureMembersTable();

    // 세션 검증 후 현재 회원 상태 재조회에 사용하는 단건 SQL
    static const std::string sql =
        "SELECT "
        "member_id, email, password_hash, name, role, status, level, exp, "
        "COALESCE(DATE_FORMAT(last_login_at, '%Y-%m-%d %H:%i:%s'), '') AS "
        "last_login_at "
        "FROM members WHERE member_id = ? LIMIT 1";

    // memberId를 바인딩해 안전하게 조회한다.
    return querySingleMemberById(sql, memberId);
}

std::optional<MemberModel> MemberMapper::createMember(const std::string &email,
                                                      const std::string &passwordHash,
                                                      const std::string &name) const
{
    // 로컬 개발 환경에서 스키마가 없는 경우를 대비해 테이블을 보장한다.
    ensureMembersTable();

    // 비밀번호 해시는 서비스 레이어에서 생성되며,
    // Mapper는 전달받은 값을 DB에 저장만 수행한다.
    const auto dbClient = drogon::app().getDbClient("default");
    dbClient->execSqlSync(
        "INSERT INTO members (email, password_hash, name) VALUES (?, ?, ?)",
        email,
        passwordHash,
        name);

    // 삽입 직후 이메일로 다시 조회해 생성된 row를 반환한다.
    return findByEmail(email);
}

void MemberMapper::updateLastLoginAt(std::uint64_t memberId) const
{
    // 로그인 성공 시점을 기록하기 위해 마지막 로그인 시각을 NOW()로 갱신한다.
    const auto dbClient = drogon::app().getDbClient("default");
    dbClient->execSqlSync(
        "UPDATE members SET last_login_at = NOW() WHERE member_id = ?",
        memberId);
}

void MemberMapper::updateProfile(std::uint64_t memberId,
                                 const std::string &email,
                                 const std::string &name) const
{
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
    // 활동 결과로 계산된 레벨/경험치 값을 회원 row에 반영한다.
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
    // API 응답에 노출 가능한 필드만 DTO에 복사한다.
    // passwordHash는 민감 정보이므로 DTO에 포함하지 않는다.
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
    // 프로세스 생애주기 동안 DDL을 1회만 실행하기 위한 플래그
    static std::atomic_bool tableReady{false};
    if (tableReady.load())
    {
        return;
    }

    // 로컬 개발 환경에서 바로 로그인/회원가입 테스트가 가능하도록
    // members 최소 스키마를 자동 생성한다.
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

    // 이후 호출에서는 DDL을 건너뛴다.
    tableReady.store(true);
}

void MemberMapper::ensureMemberFoodMbtiTable() const
{
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
    // 문자열 파라미터를 prepared statement로 바인딩해 SQL injection을 방지한다.
    const auto dbClient = drogon::app().getDbClient("default");
    const auto result = dbClient->execSqlSync(sql, value);

    // 결과가 없으면 nullopt를 반환해 호출 측에서 분기하도록 한다.
    if (result.empty())
    {
        return std::nullopt;
    }

    // 단건 조회이므로 첫 row만 모델로 변환해 반환한다.
    return rowToMemberModel(result[0]);
}

std::optional<MemberModel> MemberMapper::querySingleMemberById(
    const std::string &sql,
    std::uint64_t memberId) const
{
    // 숫자 ID 파라미터를 바인딩해 단건 조회를 수행한다.
    const auto dbClient = drogon::app().getDbClient("default");
    const auto result = dbClient->execSqlSync(sql, memberId);

    // 결과가 없으면 nullopt를 반환해 호출 측에서 분기하도록 한다.
    if (result.empty())
    {
        return std::nullopt;
    }

    // 단건 조회이므로 첫 row만 모델로 변환해 반환한다.
    return rowToMemberModel(result[0]);
}

}  // namespace auth
