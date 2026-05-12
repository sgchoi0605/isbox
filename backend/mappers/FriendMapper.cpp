#include "FriendMapper.h"

#include <drogon/drogon.h>

#include <algorithm>
#include <atomic>
#include <cctype>
#include <utility>

namespace
{

std::string toLowerCopy(std::string value)
{
    std::transform(value.begin(), value.end(), value.begin(), [](unsigned char ch) {
        return static_cast<char>(std::tolower(ch));
    });
    return value;
}

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

std::optional<FriendMemberDTO> FriendMapper::findMemberByEmail(
    const std::string &email) const
{
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

std::optional<FriendRelationshipModel> FriendMapper::findRelationship(
    std::uint64_t memberA,
    std::uint64_t memberB) const
{
    ensureFriendsTable();

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

    return findRelationship(requesterId, addresseeId);
}

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

FriendMemberDTO FriendMapper::rowToMemberDTO(const drogon::orm::Row &row)
{
    FriendMemberDTO dto;
    dto.memberId = row["member_id"].as<std::uint64_t>();
    dto.name = row["name"].as<std::string>();
    dto.email = row["email"].as<std::string>();
    dto.status = row["status"].as<std::string>();
    return dto;
}

FriendRelationshipModel FriendMapper::rowToRelationshipModel(
    const drogon::orm::Row &row)
{
    FriendRelationshipModel model;
    model.friendshipId = row["friendship_id"].as<std::uint64_t>();
    model.requesterId = row["requester_id"].as<std::uint64_t>();
    model.addresseeId = row["addressee_id"].as<std::uint64_t>();
    model.status = row["status"].as<std::string>();
    model.requestedAt = row["requested_at"].as<std::string>();

    const auto respondedAt = row["responded_at"].as<std::string>();
    if (!respondedAt.empty())
    {
        model.respondedAt = respondedAt;
    }

    return model;
}

FriendDTO FriendMapper::rowToFriendDTO(const drogon::orm::Row &row)
{
    FriendDTO dto;
    dto.friendshipId = row["friendship_id"].as<std::uint64_t>();
    dto.memberId = row["member_id"].as<std::uint64_t>();
    dto.name = row["name"].as<std::string>();
    dto.email = row["email"].as<std::string>();
    dto.status = toClientStatus(row["status"].as<std::string>());
    dto.requestedAt = row["requested_at"].as<std::string>();

    const auto respondedAt = row["responded_at"].as<std::string>();
    if (!respondedAt.empty())
    {
        dto.respondedAt = respondedAt;
    }

    return dto;
}

FriendRequestDTO FriendMapper::rowToRequestDTO(const drogon::orm::Row &row)
{
    FriendRequestDTO dto;
    dto.friendshipId = row["friendship_id"].as<std::uint64_t>();
    dto.memberId = row["member_id"].as<std::uint64_t>();
    dto.name = row["name"].as<std::string>();
    dto.email = row["email"].as<std::string>();
    dto.status = toClientStatus(row["status"].as<std::string>());
    dto.requestedAt = row["requested_at"].as<std::string>();

    const auto respondedAt = row["responded_at"].as<std::string>();
    if (!respondedAt.empty())
    {
        dto.respondedAt = respondedAt;
    }

    return dto;
}

std::string FriendMapper::toClientStatus(const std::string &dbStatus)
{
    return toLowerCopy(dbStatus);
}

}  // namespace friendship
