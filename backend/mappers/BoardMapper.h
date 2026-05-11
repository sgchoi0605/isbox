#pragma once

#include <drogon/orm/Row.h>

#include <optional>
#include <string>
#include <vector>

#include "../models/BoardTypes.h"

namespace board
{

// 게시판 DB 접근 계층이다.
// SQL 실행, ORM 삽입, DB 행(row)을 클라이언트 DTO로 변환하는 작업을 담당한다.
class BoardMapper
{
  public:
    BoardMapper() = default;

    // 최신 게시글을 최대 50개 조회한다.
    // dbTypeFilter가 있으면 DB enum 값(SHARE/EXCHANGE)으로 유형을 제한한다.
    std::vector<BoardPostDTO> findPosts(
        const std::optional<std::string> &dbTypeFilter) const;

    // 단일 게시글을 ID로 조회한다.
    // 삭제 권한 확인이나 생성 직후 재조회처럼 정확히 한 건이 필요할 때 사용한다.
    std::optional<BoardPostDTO> findById(std::uint64_t postId) const;

    // BoardPostModel을 통해 새 게시글을 삽입하고, 생성된 ID로 다시 조회해 DTO를 반환한다.
    std::optional<BoardPostDTO> insertPost(
        std::uint64_t memberId,
        const CreateBoardPostRequestDTO &request) const;

    // 행을 실제로 삭제하지 않고 status만 DELETED로 바꾸는 소프트 삭제를 수행한다.
    void markPostDeleted(std::uint64_t postId) const;

  private:
    // members 조인 결과와 board_posts 컬럼을 BoardPostDTO 하나로 합친다.
    static BoardPostDTO rowToBoardPostDTO(const drogon::orm::Row &row);

    // DB enum 문자열을 프론트엔드에서 쓰는 소문자 type 값으로 변환한다.
    static std::string toClientType(const std::string &dbType);

    // DB enum 문자열을 프론트엔드에서 쓰는 소문자 status 값으로 변환한다.
    static std::string toClientStatus(const std::string &dbStatus);

    // 로컬 개발 환경에서 테이블이 없을 때 최소 스키마를 자동 생성한다.
    void ensureTablesForLocalDev() const;
};

}  // namespace board
