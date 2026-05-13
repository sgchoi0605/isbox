#include "FriendMapper.h"

// Drogon 애플리케이션 객체와 DB 클라이언트를 사용하기 위해 포함한다.
#include <drogon/drogon.h>

// 문자열 변환, 원자 변수, 문자 처리, pair 유틸리티를 사용하기 위해 포함한다.
#include <algorithm>
#include <atomic>
#include <cctype>
#include <utility>

namespace
{

// 전달받은 문자열을 소문자로 정규화한 복사본을 반환한다.
std::string toLowerCopy(std::string value)
{
    // 문자열의 각 문자를 순회하면서 같은 버퍼에 소문자 결과를 다시 기록한다.
    std::transform(value.begin(), value.end(), value.begin(), [](unsigned char ch) {
        // std::tolower 반환값을 char로 명시적 캐스팅해 타입을 맞춘다.
        return static_cast<char>(std::tolower(ch));
    });

    // 변환이 끝난 문자열을 호출자에게 돌려준다.
    return value;
}

// 두 회원 ID를 작은 값, 큰 값 순서로 고정해 반환한다.
std::pair<std::uint64_t, std::uint64_t> orderedPair(std::uint64_t a,
                                                     std::uint64_t b)
{
    // 첫 번째 값이 더 작으면 그대로 (a, b)를 사용한다.
    if (a < b)
    {
        return {a, b};
    }

    // 그렇지 않으면 순서를 바꿔 (b, a)로 반환한다.
    return {b, a};
}

}  // 익명 네임스페이스

namespace friendship
{

// 이메일로 단일 회원 정보를 조회한다.
std::optional<FriendMemberDTO> FriendMapper::findMemberByEmail(
    const std::string &email) const
{
    // 친구 관련 테이블이 없으면 먼저 생성되도록 보장한다.
    ensureFriendsTable();

    // 기본 DB 커넥션 풀에서 동기 SQL 실행용 클라이언트를 가져온다.
    const auto dbClient = drogon::app().getDbClient("default");

    // 이메일이 일치하는 회원 1건만 조회하는 SQL을 준비한다.
    static const std::string sql =
        "SELECT member_id, name, email, status "
        "FROM members WHERE email = ? LIMIT 1";

    // 바인딩 파라미터로 이메일을 전달해 조회를 실행한다.
    const auto result = dbClient->execSqlSync(sql, email);

    // 조회 결과가 없으면 값을 찾지 못했음을 나타내기 위해 nullopt를 반환한다.
    if (result.empty())
    {
        return std::nullopt;
    }

    // 첫 번째 행을 DTO로 변환해 반환한다.
    return rowToMemberDTO(result[0]);
}

// 두 회원 사이의 친구 관계를 회원 ID 쌍 기준으로 조회한다.
std::optional<FriendRelationshipModel> FriendMapper::findRelationship(
    std::uint64_t memberA,
    std::uint64_t memberB) const
{
    // 친구 관련 테이블이 없으면 먼저 생성되도록 보장한다.
    ensureFriendsTable();

    // 중복 쌍(a,b)/(b,a)을 같은 키로 취급하기 위해 정렬된 ID 쌍을 만든다.
    const auto [smallerId, largerId] = orderedPair(memberA, memberB);

    // 기본 DB 커넥션 풀에서 동기 SQL 실행용 클라이언트를 가져온다.
    const auto dbClient = drogon::app().getDbClient("default");

    // 정렬된 회원 쌍 기준으로 친구 관계 1건을 조회하는 SQL을 준비한다.
    static const std::string sql =
        "SELECT "
        "friendship_id, requester_id, addressee_id, status, "
        "COALESCE(DATE_FORMAT(requested_at, '%Y-%m-%d %H:%i:%s'), '') AS requested_at, "
        "COALESCE(DATE_FORMAT(responded_at, '%Y-%m-%d %H:%i:%s'), '') AS responded_at "
        "FROM member_friends "
        "WHERE smaller_member_id = ? AND larger_member_id = ? "
        "LIMIT 1";

    // 정렬된 회원 ID 2개를 바인딩해 조회를 실행한다.
    const auto result = dbClient->execSqlSync(sql, smallerId, largerId);

    // 결과가 없으면 관계가 없다고 판단한다.
    if (result.empty())
    {
        return std::nullopt;
    }

    // 조회한 한 행을 관계 모델로 변환해 반환한다.
    return rowToRelationshipModel(result[0]);
}

// friendship_id(관계 PK)로 단일 친구 관계를 조회한다.
std::optional<FriendRelationshipModel> FriendMapper::findRelationshipById(
    std::uint64_t friendshipId) const
{
    // 친구 관련 테이블이 없으면 먼저 생성되도록 보장한다.
    ensureFriendsTable();

    // 기본 DB 커넥션 풀에서 동기 SQL 실행용 클라이언트를 가져온다.
    const auto dbClient = drogon::app().getDbClient("default");

    // PK 기준 단건 조회 SQL을 준비한다.
    static const std::string sql =
        "SELECT "
        "friendship_id, requester_id, addressee_id, status, "
        "COALESCE(DATE_FORMAT(requested_at, '%Y-%m-%d %H:%i:%s'), '') AS requested_at, "
        "COALESCE(DATE_FORMAT(responded_at, '%Y-%m-%d %H:%i:%s'), '') AS responded_at "
        "FROM member_friends "
        "WHERE friendship_id = ? "
        "LIMIT 1";

    // friendship_id를 바인딩해 조회를 실행한다.
    const auto result = dbClient->execSqlSync(sql, friendshipId);

    // 결과가 없으면 nullopt를 반환한다.
    if (result.empty())
    {
        return std::nullopt;
    }

    // 첫 행을 모델로 변환해 반환한다.
    return rowToRelationshipModel(result[0]);
}

// 친구 요청을 생성하거나(없을 때) 기존 쌍을 대기 상태로 재설정한다.
std::optional<FriendRelationshipModel> FriendMapper::createRequest(
    std::uint64_t requesterId,
    std::uint64_t addresseeId) const
{
    // 친구 관련 테이블이 없으면 먼저 생성되도록 보장한다.
    ensureFriendsTable();

    // 유니크 키(smaller_member_id, larger_member_id)에 맞추기 위해 정렬된 ID 쌍을 만든다.
    const auto [smallerId, largerId] = orderedPair(requesterId, addresseeId);

    // 기본 DB 커넥션 풀에서 동기 SQL 실행용 클라이언트를 가져온다.
    const auto dbClient = drogon::app().getDbClient("default");

    // 동일한 회원 쌍이 있으면 덮어쓰고, 없으면 새로 삽입한다.
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

    // 저장 후 최신 관계 상태를 다시 조회해 반환한다.
    return findRelationship(requesterId, addresseeId);
}

// 수신자가 대기 중 요청을 수락한다.
bool FriendMapper::acceptRequest(std::uint64_t friendshipId,
                                 std::uint64_t addresseeId) const
{
    // 친구 관련 테이블이 없으면 먼저 생성되도록 보장한다.
    ensureFriendsTable();

    // 기본 DB 커넥션 풀에서 동기 SQL 실행용 클라이언트를 가져온다.
    const auto dbClient = drogon::app().getDbClient("default");

    // 본인(addressee)에게 온 PENDING 요청만 ACCEPTED로 변경한다.
    const auto result = dbClient->execSqlSync(
        "UPDATE member_friends "
        "SET status = 'ACCEPTED', responded_at = NOW() "
        "WHERE friendship_id = ? AND addressee_id = ? AND status = 'PENDING'",
        friendshipId,
        addresseeId);

    // 실제 변경된 행이 1건 이상이면 성공으로 본다.
    return result.affectedRows() > 0;
}

// 수신자가 대기 중 요청을 거절한다.
bool FriendMapper::rejectRequest(std::uint64_t friendshipId,
                                 std::uint64_t addresseeId) const
{
    // 친구 관련 테이블이 없으면 먼저 생성되도록 보장한다.
    ensureFriendsTable();

    // 기본 DB 커넥션 풀에서 동기 SQL 실행용 클라이언트를 가져온다.
    const auto dbClient = drogon::app().getDbClient("default");

    // 본인(addressee)에게 온 PENDING 요청만 REJECTED로 변경한다.
    const auto result = dbClient->execSqlSync(
        "UPDATE member_friends "
        "SET status = 'REJECTED', responded_at = NOW() "
        "WHERE friendship_id = ? AND addressee_id = ? AND status = 'PENDING'",
        friendshipId,
        addresseeId);

    // 실제 변경된 행이 1건 이상이면 성공으로 본다.
    return result.affectedRows() > 0;
}

// 요청자가 자신이 보낸 대기 중 요청을 취소한다.
bool FriendMapper::cancelRequest(std::uint64_t friendshipId,
                                 std::uint64_t requesterId) const
{
    // 친구 관련 테이블이 없으면 먼저 생성되도록 보장한다.
    ensureFriendsTable();

    // 기본 DB 커넥션 풀에서 동기 SQL 실행용 클라이언트를 가져온다.
    const auto dbClient = drogon::app().getDbClient("default");

    // 본인(requester)이 보낸 PENDING 요청만 CANCELED로 변경한다.
    const auto result = dbClient->execSqlSync(
        "UPDATE member_friends "
        "SET status = 'CANCELED', responded_at = NOW() "
        "WHERE friendship_id = ? AND requester_id = ? AND status = 'PENDING'",
        friendshipId,
        requesterId);

    // 실제 변경된 행이 1건 이상이면 성공으로 본다.
    return result.affectedRows() > 0;
}

// 특정 회원의 수락된 친구 목록을 조회한다.
std::vector<FriendDTO> FriendMapper::listAcceptedFriends(std::uint64_t memberId) const
{
    // 친구 관련 테이블이 없으면 먼저 생성되도록 보장한다.
    ensureFriendsTable();

    // 기본 DB 커넥션 풀에서 동기 SQL 실행용 클라이언트를 가져온다.
    const auto dbClient = drogon::app().getDbClient("default");

    // requester/addressee 중 현재 회원이 아닌 "상대 회원" 정보를 CASE로 선택해 조회한다.
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

    // CASE 3개 + WHERE 2개에 같은 memberId를 바인딩해 실행한다.
    const auto result =
        dbClient->execSqlSync(sql, memberId, memberId, memberId, memberId, memberId);

    // 결과 행 수만큼 미리 메모리를 확보해 push_back 재할당 비용을 줄인다.
    std::vector<FriendDTO> out;
    out.reserve(result.size());

    // 각 행을 FriendDTO로 변환해 반환 벡터에 누적한다.
    for (const auto &row : result)
    {
        out.push_back(rowToFriendDTO(row));
    }

    // 완성된 친구 목록을 반환한다.
    return out;
}

// 특정 회원이 받은(PENDING) 친구 요청 목록을 조회한다.
std::vector<FriendRequestDTO> FriendMapper::listIncomingRequests(
    std::uint64_t memberId) const
{
    // 친구 관련 테이블이 없으면 먼저 생성되도록 보장한다.
    ensureFriendsTable();

    // 기본 DB 커넥션 풀에서 동기 SQL 실행용 클라이언트를 가져온다.
    const auto dbClient = drogon::app().getDbClient("default");

    // requester 회원 정보를 JOIN해서 "누가 보냈는지"를 포함해 조회한다.
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

    // 현재 회원 ID를 수신자 조건에 바인딩해 실행한다.
    const auto result = dbClient->execSqlSync(sql, memberId);

    // 결과 행 수만큼 미리 메모리를 확보해 push_back 재할당 비용을 줄인다.
    std::vector<FriendRequestDTO> out;
    out.reserve(result.size());

    // 각 행을 요청 DTO로 변환해 반환 벡터에 누적한다.
    for (const auto &row : result)
    {
        out.push_back(rowToRequestDTO(row));
    }

    // 완성된 수신 요청 목록을 반환한다.
    return out;
}

// 특정 회원이 보낸(PENDING) 친구 요청 목록을 조회한다.
std::vector<FriendRequestDTO> FriendMapper::listOutgoingRequests(
    std::uint64_t memberId) const
{
    // 친구 관련 테이블이 없으면 먼저 생성되도록 보장한다.
    ensureFriendsTable();

    // 기본 DB 커넥션 풀에서 동기 SQL 실행용 클라이언트를 가져온다.
    const auto dbClient = drogon::app().getDbClient("default");

    // addressee 회원 정보를 JOIN해서 "누구에게 보냈는지"를 포함해 조회한다.
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

    // 현재 회원 ID를 발신자 조건에 바인딩해 실행한다.
    const auto result = dbClient->execSqlSync(sql, memberId);

    // 결과 행 수만큼 미리 메모리를 확보해 push_back 재할당 비용을 줄인다.
    std::vector<FriendRequestDTO> out;
    out.reserve(result.size());

    // 각 행을 요청 DTO로 변환해 반환 벡터에 누적한다.
    for (const auto &row : result)
    {
        out.push_back(rowToRequestDTO(row));
    }

    // 완성된 발신 요청 목록을 반환한다.
    return out;
}

// 특정 friendship_id를 현재 회원 관점의 요청 DTO 형태로 조회한다.
std::optional<FriendRequestDTO> FriendMapper::findRequestViewForMember(
    std::uint64_t friendshipId,
    std::uint64_t memberId) const
{
    // 친구 관련 테이블이 없으면 먼저 생성되도록 보장한다.
    ensureFriendsTable();

    // 기본 DB 커넥션 풀에서 동기 SQL 실행용 클라이언트를 가져온다.
    const auto dbClient = drogon::app().getDbClient("default");

    // 현재 회원이 requester인지 여부에 따라 "상대 회원" 컬럼을 CASE로 선택한다.
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

    // CASE 파라미터 3개 + friendship_id 1개 + 소유권 검증 2개를 순서대로 바인딩한다.
    const auto result = dbClient->execSqlSync(
        sql,
        memberId,
        memberId,
        memberId,
        friendshipId,
        memberId,
        memberId);

    // 조회 결과가 없으면 해당 회원이 볼 수 없는 요청으로 판단해 nullopt를 반환한다.
    if (result.empty())
    {
        return std::nullopt;
    }

    // 첫 행을 요청 DTO로 변환해 반환한다.
    return rowToRequestDTO(result[0]);
}

// friends 관련 테이블/제약조건이 준비되어 있는지 1회 보장한다.
void FriendMapper::ensureFriendsTable() const
{
    // 프로세스 내에서 테이블 준비 여부를 공유하는 원자 플래그다.
    static std::atomic_bool tableReady{false};

    // 이미 준비된 상태라면 즉시 반환해 중복 DDL 실행을 피한다.
    if (tableReady.load())
    {
        return;
    }

    // 기본 DB 커넥션 풀에서 동기 SQL 실행용 클라이언트를 가져온다.
    const auto dbClient = drogon::app().getDbClient("default");

    // members 테이블이 없을 때만 생성한다.
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

    // member_friends 테이블이 없을 때만 생성한다.
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

    // 이후 호출부터는 DDL을 다시 실행하지 않도록 준비 완료 상태로 표시한다.
    tableReady.store(true);
}

// members 조회 행을 FriendMemberDTO로 변환한다.
FriendMemberDTO FriendMapper::rowToMemberDTO(const drogon::orm::Row &row)
{
    // 반환할 DTO 인스턴스를 생성한다.
    FriendMemberDTO dto;

    // 각 컬럼을 DTO 필드에 타입에 맞게 매핑한다.
    dto.memberId = row["member_id"].as<std::uint64_t>();
    dto.name = row["name"].as<std::string>();
    dto.email = row["email"].as<std::string>();
    dto.status = row["status"].as<std::string>();

    // 매핑이 완료된 DTO를 반환한다.
    return dto;
}

// member_friends 조회 행을 FriendRelationshipModel로 변환한다.
FriendRelationshipModel FriendMapper::rowToRelationshipModel(
    const drogon::orm::Row &row)
{
    // 반환할 모델 인스턴스를 생성한다.
    FriendRelationshipModel model;

    // 관계 식별자/양측 회원/상태/요청 시각을 먼저 채운다.
    model.friendshipId = row["friendship_id"].as<std::uint64_t>();
    model.requesterId = row["requester_id"].as<std::uint64_t>();
    model.addresseeId = row["addressee_id"].as<std::uint64_t>();
    model.status = row["status"].as<std::string>();
    model.requestedAt = row["requested_at"].as<std::string>();

    // responded_at은 빈 문자열일 수 있으므로 임시 변수로 읽는다.
    const auto respondedAt = row["responded_at"].as<std::string>();

    // 실제 값이 있을 때만 optional 필드에 채운다.
    if (!respondedAt.empty())
    {
        model.respondedAt = respondedAt;
    }

    // 매핑이 완료된 모델을 반환한다.
    return model;
}

// 친구 목록 조회 행을 FriendDTO로 변환한다.
FriendDTO FriendMapper::rowToFriendDTO(const drogon::orm::Row &row)
{
    // 반환할 DTO 인스턴스를 생성한다.
    FriendDTO dto;

    // 기본 식별자/사용자 정보 필드를 매핑한다.
    dto.friendshipId = row["friendship_id"].as<std::uint64_t>();
    dto.memberId = row["member_id"].as<std::uint64_t>();
    dto.name = row["name"].as<std::string>();
    dto.email = row["email"].as<std::string>();

    // DB 상태(대문자)를 클라이언트 상태(소문자)로 정규화해 넣는다.
    dto.status = toClientStatus(row["status"].as<std::string>());

    // 요청 시각 문자열을 그대로 매핑한다.
    dto.requestedAt = row["requested_at"].as<std::string>();

    // responded_at은 빈 문자열일 수 있으므로 임시 변수로 읽는다.
    const auto respondedAt = row["responded_at"].as<std::string>();

    // 실제 값이 있을 때만 optional 필드에 채운다.
    if (!respondedAt.empty())
    {
        dto.respondedAt = respondedAt;
    }

    // 매핑이 완료된 DTO를 반환한다.
    return dto;
}

// 요청 목록 조회 행을 FriendRequestDTO로 변환한다.
FriendRequestDTO FriendMapper::rowToRequestDTO(const drogon::orm::Row &row)
{
    // 반환할 DTO 인스턴스를 생성한다.
    FriendRequestDTO dto;

    // 기본 식별자/사용자 정보 필드를 매핑한다.
    dto.friendshipId = row["friendship_id"].as<std::uint64_t>();
    dto.memberId = row["member_id"].as<std::uint64_t>();
    dto.name = row["name"].as<std::string>();
    dto.email = row["email"].as<std::string>();

    // DB 상태(대문자)를 클라이언트 상태(소문자)로 정규화해 넣는다.
    dto.status = toClientStatus(row["status"].as<std::string>());

    // 요청 시각 문자열을 그대로 매핑한다.
    dto.requestedAt = row["requested_at"].as<std::string>();

    // responded_at은 빈 문자열일 수 있으므로 임시 변수로 읽는다.
    const auto respondedAt = row["responded_at"].as<std::string>();

    // 실제 값이 있을 때만 optional 필드에 채운다.
    if (!respondedAt.empty())
    {
        dto.respondedAt = respondedAt;
    }

    // 매핑이 완료된 DTO를 반환한다.
    return dto;
}

// DB 상태 문자열을 클라이언트 응답용 상태 문자열로 변환한다.
std::string FriendMapper::toClientStatus(const std::string &dbStatus)
{
    // 현재 구현은 소문자 변환만 수행한다.
    return toLowerCopy(dbStatus);
}

}  // friendship 네임스페이스
