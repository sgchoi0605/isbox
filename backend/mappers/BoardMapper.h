#pragma once

#include <drogon/orm/Row.h>

#include <optional>
#include <string>
#include <vector>

#include "../models/BoardTypes.h"

namespace board
{

// board_posts 테이블(및 members 조인)을 다루는 데이터 접근 계층이다.
// 서비스 레이어는 SQL 대신 Mapper 함수 호출로 게시글 데이터를 처리한다.
class BoardMapper
{
  public:
    // 별도 상태가 없는 Mapper이므로 기본 생성자를 사용한다.
    BoardMapper() = default;

    // 최신 게시글 목록을 최대 50건 조회한다.
    // dbTypeFilter가 있으면 DB enum(SHARE/EXCHANGE) 기준으로 타입을 제한한다.
    std::vector<BoardPostDTO> findPosts(
        const std::optional<std::string> &dbTypeFilter) const;

    // 게시글 ID로 단건 조회한다.
    // 삭제 권한 확인, 생성 직후 재조회 같은 흐름에서 사용한다.
    std::optional<BoardPostDTO> findById(std::uint64_t postId) const;

    // 새 게시글을 삽입하고, 삽입된 게시글을 DTO 형태로 다시 조회해 반환한다.
    std::optional<BoardPostDTO> insertPost(
        std::uint64_t memberId,
        const CreateBoardPostRequestDTO &request) const;

    // 물리 삭제 대신 status를 DELETED로 변경하는 소프트 삭제를 수행한다.
    void markPostDeleted(std::uint64_t postId) const;

  private:
    // board_posts + members 조인 row를 BoardPostDTO로 변환한다.
    static BoardPostDTO rowToBoardPostDTO(const drogon::orm::Row &row);

    // DB enum 문자열을 API 응답용 type 값으로 변환한다.
    static std::string toClientType(const std::string &dbType);

    // DB enum 문자열을 API 응답용 status 값으로 변환한다.
    static std::string toClientStatus(const std::string &dbStatus);

    // 로컬 개발 환경에서 테이블이 없을 경우 최소 스키마를 자동 생성한다.
    void ensureTablesForLocalDev() const;
};

}  // namespace board
