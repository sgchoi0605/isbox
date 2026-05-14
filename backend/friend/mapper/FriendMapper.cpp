/*
 * 파일 역할 요약:
 * - FriendMapper의 SQL 실행 로직과 Row->DTO 변환 로직을 구현한다.
 * - 개발/로컬 환경에서 필요한 테이블 준비(ensure) 로직도 함께 담당한다.
 */
#include "friend/mapper/FriendMapper.h"

// Drogon 앱 컨텍스트와 기본 DB 클라이언트를 사용한다.
#include <drogon/drogon.h>

#include <algorithm>
#include <atomic>
#include <cctype>
#include <utility>

namespace
{

// DB 상태 문자열을 소문자로 정규화한다.
std::string toLowerCopy(std::string value)
{
    std::transform(value.begin(), value.end(), value.begin(), [](unsigned char ch) {
        return static_cast<char>(std::tolower(ch));
    });

    return value;
}

// 회원 쌍을 항상 (작은 ID, 큰 ID) 순으로 고정한다.
std::pair<std::uint64_t, std::uint64_t> orderedPair(std::uint64_t a,
                                                     std::uint64_t b)
{
    if (a < b)
    {
        return {a, b};
    }

    return {b, a};
}

}  // namespace

namespace friendship
{

// 이메일로 회원 1건을 조회한다.
std::optional<FriendMemberDTO> FriendMapper::findMemberByEmail(
    const std::string &email) const
{
    // 테이블/인덱스가 없으면 먼저 보장한다.
    ensureFriendsTable();

    const auto dbClient = drogon::app().getDbClient("default");

    static const std::string sql =
        "SELECT member_id, name, email, status "
        "FROM members WHERE email = ? LIMIT 1";

    const auto result = dbClient->execSqlSync(sql, email);

    if (result.empty())
    {
        return std::nullopt;
    }

    return rowToMemberDTO(result[0]);
}

// 두 회원 사이의 친구 관계를 조회한다.
std::optional<FriendRelationshipModel> FriendMapper::findRelationship(
    std::uint64_t memberA,
    std::uint64_t memberB) const
{
    ensureFriendsTable();

    // 유니크 키 규칙에 맞게 회원 쌍을 정렬한다.
    const auto [smallerId, largerId] = orderedPair(memberA, memberB);

    const auto dbClient = drogon::app().getDbClient("default");

    static const std::string sql =
        "SELECT "
        "friendship_id, requester_id, addressee_id, status, "
        "COALESCE(DATE_FORMAT(requested_at, '%Y-%m-%d %H:%i:%s'), '') AS requested_at, "
        "COALESCE(DATE_FORMAT(responded_at, '%Y-%m-%d %H:%i:%s'), '') AS responded_at "
        "FROM member_friends "
        "WHERE smaller_member_id = ? AND larger_member_id = ? "
        "LIMIT 1";

    const auto result = dbClient->execSqlSync(sql, smallerId, largerId);

    if (result.empty())
    {
        return std::nullopt;
    }

    return rowToRelationshipModel(result[0]);
}

// friendship_id로 친구 관계 1건을 조회한다.
std::optional<FriendRelationshipModel> FriendMapper::findRelationshipById(
    std::uint64_t friendshipId) const
{
    ensureFriendsTable();

    const auto dbClient = drogon::app().getDbClient("default");

    static const std::string sql =
        "SELECT "
        "friendship_id, requester_id, addressee_id, status, "
        "COALESCE(DATE_FORMAT(requested_at, '%Y-%m-%d %H:%i:%s'), '') AS requested_at, "
        "COALESCE(DATE_FORMAT(responded_at, '%Y-%m-%d %H:%i:%s'), '') AS responded_at "
        "FROM member_friends "
        "WHERE friendship_id = ? "
        "LIMIT 1";

    const auto result = dbClient->execSqlSync(sql, friendshipId);

    if (result.empty())
    {
        return std::nullopt;
    }

    return rowToRelationshipModel(result[0]);
}

// 친구 요청을 생성한다.
// 같은 회원 쌍이 이미 존재하면 PENDING 상태로 갱신한다.
std::optional<FriendRelationshipModel> FriendMapper::createRequest(
    std::uint64_t requesterId,
    std::uint64_t addresseeId) const
{
    ensureFriendsTable();

    const auto [smallerId, largerId] = orderedPair(requesterId, addresseeId);

    const auto dbClient = drogon::app().getDbClient("default");

    dbClient->execSqlSync(
        "INSERT INTO member_friends ("
        "requester_id, addressee_id, smaller_member_id, larger_member_id, status, requested_at, responded_at"
        ") VALUES (?, ?, ?, ?, 'PENDING', NOW(), NULL) "
        "ON DUPLICATE KEY UPDATE "
        "requester_id = VALUES(requester_id), "
        "addressee_id = VALUES(addressee_id), "
        "status = 'PENDING', "
        "requested_at = NOW(), "
        "responded_at = NULL",
        requesterId,
        addresseeId,
        smallerId,
        largerId);

    // 변경 이후 최신 관계 상태를 재조회해 반환한다.
    return findRelationship(requesterId, addresseeId);
}

// 수신자 본인의 PENDING 요청을 ACCEPTED로 변경한다.
bool FriendMapper::acceptRequest(std::uint64_t friendshipId,
                                 std::uint64_t addresseeId) const
{
    ensureFriendsTable();

    const auto dbClient = drogon::app().getDbClient("default");

    const auto result = dbClient->execSqlSync(
        "UPDATE member_friends "
        "SET status = 'ACCEPTED', responded_at = NOW() "
        "WHERE friendship_id = ? AND addressee_id = ? AND status = 'PENDING'",
        friendshipId,
        addresseeId);

    return result.affectedRows() > 0;
}

// 수신자 본인의 PENDING 요청을 REJECTED로 변경한다.
bool FriendMapper::rejectRequest(std::uint64_t friendshipId,
                                 std::uint64_t addresseeId) const
{
    ensureFriendsTable();

    const auto dbClient = drogon::app().getDbClient("default");

    const auto result = dbClient->execSqlSync(
        "UPDATE member_friends "
        "SET status = 'REJECTED', responded_at = NOW() "
        "WHERE friendship_id = ? AND addressee_id = ? AND status = 'PENDING'",
        friendshipId,
        addresseeId);

    return result.affectedRows() > 0;
}

// 요청자 본인의 PENDING 요청을 CANCELED로 변경한다.
bool FriendMapper::cancelRequest(std::uint64_t friendshipId,
                                 std::uint64_t requesterId) const
{
    ensureFriendsTable();

    const auto dbClient = drogon::app().getDbClient("default");

    const auto result = dbClient->execSqlSync(
        "UPDATE member_friends "
        "SET status = 'CANCELED', responded_at = NOW() "
        "WHERE friendship_id = ? AND requester_id = ? AND status = 'PENDING'",
        friendshipId,
        requesterId);

    return result.affectedRows() > 0;
}

// 수락된 친구 관계를 삭제한다.
bool FriendMapper::removeAcceptedFriendship(std::uint64_t friendshipId,
                                            std::uint64_t memberId) const
{
    ensureFriendsTable();

    const auto dbClient = drogon::app().getDbClient("default");

    const auto result = dbClient->execSqlSync(
        "DELETE FROM member_friends "
        "WHERE friendship_id = ? "
        "AND (requester_id = ? OR addressee_id = ?) "
        "AND status = 'ACCEPTED'",
        friendshipId,
        memberId,
        memberId);

    return result.affectedRows() > 0;
}

// 수락된 친구 목록을 조회한다.
// 현재 회원이 아닌 상대 회원 정보를 CASE 구문으로 선택한다.
std::vector<FriendDTO> FriendMapper::listAcceptedFriends(std::uint64_t memberId) const
{
    ensureFriendsTable();

    const auto dbClient = drogon::app().getDbClient("default");

    static const std::string sql =
        "SELECT "
        "f.friendship_id, "
        "CASE WHEN f.requester_id = ? THEN m2.member_id ELSE m1.member_id END AS member_id, "
        "CASE WHEN f.requester_id = ? THEN m2.name ELSE m1.name END AS name, "
        "CASE WHEN f.requester_id = ? THEN m2.email ELSE m1.email END AS email, "
        "f.status, "
        "COALESCE(DATE_FORMAT(f.requested_at, '%Y-%m-%d %H:%i:%s'), '') AS requested_at, "
        "COALESCE(DATE_FORMAT(f.responded_at, '%Y-%m-%d %H:%i:%s'), '') AS responded_at "
        "FROM member_friends f "
        "JOIN members m1 ON m1.member_id = f.requester_id "
        "JOIN members m2 ON m2.member_id = f.addressee_id "
        "WHERE (f.requester_id = ? OR f.addressee_id = ?) "
        "AND f.status = 'ACCEPTED' "
        "ORDER BY f.responded_at DESC, f.requested_at DESC";

    const auto result =
        dbClient->execSqlSync(sql, memberId, memberId, memberId, memberId, memberId);

    std::vector<FriendDTO> out;
    out.reserve(result.size());

    for (const auto &row : result)
    {
        out.push_back(rowToFriendDTO(row));
    }

    return out;
}

// 받은(PENDING) 친구 요청 목록을 조회한다.
std::vector<FriendRequestDTO> FriendMapper::listIncomingRequests(
    std::uint64_t memberId) const
{
    ensureFriendsTable();

    const auto dbClient = drogon::app().getDbClient("default");

    static const std::string sql =
        "SELECT "
        "f.friendship_id, "
        "m.member_id, "
        "m.name, "
        "m.email, "
        "f.status, "
        "COALESCE(DATE_FORMAT(f.requested_at, '%Y-%m-%d %H:%i:%s'), '') AS requested_at, "
        "COALESCE(DATE_FORMAT(f.responded_at, '%Y-%m-%d %H:%i:%s'), '') AS responded_at "
        "FROM member_friends f "
        "JOIN members m ON m.member_id = f.requester_id "
        "WHERE f.addressee_id = ? AND f.status = 'PENDING' "
        "ORDER BY f.requested_at DESC";

    const auto result = dbClient->execSqlSync(sql, memberId);

    std::vector<FriendRequestDTO> out;
    out.reserve(result.size());

    for (const auto &row : result)
    {
        out.push_back(rowToRequestDTO(row));
    }

    return out;
}

// 보낸(PENDING) 친구 요청 목록을 조회한다.
std::vector<FriendRequestDTO> FriendMapper::listOutgoingRequests(
    std::uint64_t memberId) const
{
    ensureFriendsTable();

    const auto dbClient = drogon::app().getDbClient("default");

    static const std::string sql =
        "SELECT "
        "f.friendship_id, "
        "m.member_id, "
        "m.name, "
        "m.email, "
        "f.status, "
        "COALESCE(DATE_FORMAT(f.requested_at, '%Y-%m-%d %H:%i:%s'), '') AS requested_at, "
        "COALESCE(DATE_FORMAT(f.responded_at, '%Y-%m-%d %H:%i:%s'), '') AS responded_at "
        "FROM member_friends f "
        "JOIN members m ON m.member_id = f.addressee_id "
        "WHERE f.requester_id = ? AND f.status = 'PENDING' "
        "ORDER BY f.requested_at DESC";

    const auto result = dbClient->execSqlSync(sql, memberId);

    std::vector<FriendRequestDTO> out;
    out.reserve(result.size());

    for (const auto &row : result)
    {
        out.push_back(rowToRequestDTO(row));
    }

    return out;
}

// 특정 friendship_id를 현재 회원 관점의 요청 DTO로 조회한다.
std::optional<FriendRequestDTO> FriendMapper::findRequestViewForMember(
    std::uint64_t friendshipId,
    std::uint64_t memberId) const
{
    ensureFriendsTable();

    const auto dbClient = drogon::app().getDbClient("default");

    static const std::string sql =
        "SELECT "
        "f.friendship_id, "
        "CASE WHEN f.requester_id = ? THEN m2.member_id ELSE m1.member_id END AS member_id, "
        "CASE WHEN f.requester_id = ? THEN m2.name ELSE m1.name END AS name, "
        "CASE WHEN f.requester_id = ? THEN m2.email ELSE m1.email END AS email, "
        "f.status, "
        "COALESCE(DATE_FORMAT(f.requested_at, '%Y-%m-%d %H:%i:%s'), '') AS requested_at, "
        "COALESCE(DATE_FORMAT(f.responded_at, '%Y-%m-%d %H:%i:%s'), '') AS responded_at "
        "FROM member_friends f "
        "JOIN members m1 ON m1.member_id = f.requester_id "
        "JOIN members m2 ON m2.member_id = f.addressee_id "
        "WHERE f.friendship_id = ? "
        "AND (f.requester_id = ? OR f.addressee_id = ?) "
        "LIMIT 1";

    const auto result = dbClient->execSqlSync(
        sql,
        memberId,
        memberId,
        memberId,
        friendshipId,
        memberId,
        memberId);

    if (result.empty())
    {
        return std::nullopt;
    }

    return rowToRequestDTO(result[0]);
}

// 로컬/개발 환경에서 필요한 테이블과 제약을 최초 1회 준비한다.
void FriendMapper::ensureFriendsTable() const
{
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

    dbClient->execSqlSync(
        "CREATE TABLE IF NOT EXISTS member_friends ("
        "friendship_id BIGINT UNSIGNED NOT NULL AUTO_INCREMENT,"
        "requester_id BIGINT UNSIGNED NOT NULL,"
        "addressee_id BIGINT UNSIGNED NOT NULL,"
        "smaller_member_id BIGINT UNSIGNED NOT NULL,"
        "larger_member_id BIGINT UNSIGNED NOT NULL,"
        "status ENUM('PENDING', 'ACCEPTED', 'REJECTED', 'CANCELED') NOT NULL DEFAULT 'PENDING',"
        "requested_at DATETIME NOT NULL DEFAULT CURRENT_TIMESTAMP,"
        "responded_at DATETIME NULL,"
        "updated_at DATETIME NOT NULL DEFAULT CURRENT_TIMESTAMP ON UPDATE CURRENT_TIMESTAMP,"
        "PRIMARY KEY (friendship_id),"
        "UNIQUE KEY uq_member_friends_pair (smaller_member_id, larger_member_id),"
        "KEY idx_member_friends_requester_status (requester_id, status),"
        "KEY idx_member_friends_addressee_status (addressee_id, status),"
        "CONSTRAINT fk_member_friends_requester FOREIGN KEY (requester_id) "
        "REFERENCES members(member_id) ON DELETE CASCADE,"
        "CONSTRAINT fk_member_friends_addressee FOREIGN KEY (addressee_id) "
        "REFERENCES members(member_id) ON DELETE CASCADE,"
        "CONSTRAINT chk_member_friends_not_self CHECK (requester_id <> addressee_id),"
        "CONSTRAINT chk_member_friends_pair CHECK (smaller_member_id < larger_member_id)"
        ") ENGINE=InnoDB DEFAULT CHARSET=utf8mb4");

    tableReady.store(true);
}

// members 조회 행을 FriendMemberDTO로 변환한다.
FriendMemberDTO FriendMapper::rowToMemberDTO(const drogon::orm::Row &row)
{
    FriendMemberDTO dto;

    dto.memberId = row["member_id"].as<std::uint64_t>();
    dto.name = row["name"].as<std::string>();
    dto.email = row["email"].as<std::string>();
    dto.status = row["status"].as<std::string>();

    return dto;
}

// member_friends 조회 행을 FriendRelationshipModel로 변환한다.
FriendRelationshipModel FriendMapper::rowToRelationshipModel(
    const drogon::orm::Row &row)
{
    FriendRelationshipModel model;

    model.friendshipId = row["friendship_id"].as<std::uint64_t>();
    model.requesterId = row["requester_id"].as<std::uint64_t>();
    model.addresseeId = row["addressee_id"].as<std::uint64_t>();
    model.status = row["status"].as<std::string>();
    model.requestedAt = row["requested_at"].as<std::string>();

    // responded_at이 빈 문자열이면 아직 응답이 없는 상태다.
    const auto respondedAt = row["responded_at"].as<std::string>();

    if (!respondedAt.empty())
    {
        model.respondedAt = respondedAt;
    }

    return model;
}

// 친구 목록 조회 행을 FriendDTO로 변환한다.
FriendDTO FriendMapper::rowToFriendDTO(const drogon::orm::Row &row)
{
    FriendDTO dto;

    dto.friendshipId = row["friendship_id"].as<std::uint64_t>();
    dto.memberId = row["member_id"].as<std::uint64_t>();
    dto.name = row["name"].as<std::string>();
    dto.email = row["email"].as<std::string>();

    // DB 대문자 상태를 응답 소문자 상태로 변환한다.
    dto.status = toClientStatus(row["status"].as<std::string>());

    dto.requestedAt = row["requested_at"].as<std::string>();

    // responded_at이 빈 문자열이면 아직 응답이 없는 상태다.
    const auto respondedAt = row["responded_at"].as<std::string>();

    if (!respondedAt.empty())
    {
        dto.respondedAt = respondedAt;
    }

    return dto;
}

// 요청 목록 조회 행을 FriendRequestDTO로 변환한다.
FriendRequestDTO FriendMapper::rowToRequestDTO(const drogon::orm::Row &row)
{
    FriendRequestDTO dto;

    dto.friendshipId = row["friendship_id"].as<std::uint64_t>();
    dto.memberId = row["member_id"].as<std::uint64_t>();
    dto.name = row["name"].as<std::string>();
    dto.email = row["email"].as<std::string>();

    // DB 대문자 상태를 응답 소문자 상태로 변환한다.
    dto.status = toClientStatus(row["status"].as<std::string>());

    dto.requestedAt = row["requested_at"].as<std::string>();

    // responded_at이 빈 문자열이면 아직 응답이 없는 상태다.
    const auto respondedAt = row["responded_at"].as<std::string>();

    if (!respondedAt.empty())
    {
        dto.respondedAt = respondedAt;
    }

    return dto;
}

// 상태 변환 규칙을 한곳에서 관리한다.
std::string FriendMapper::toClientStatus(const std::string &dbStatus)
{
    return toLowerCopy(dbStatus);
}

}  // namespace friendship
