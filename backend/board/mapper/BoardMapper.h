/*
 * 파일 역할:
 * - board_posts/members 테이블을 대상으로 게시판 전용 DB 작업을 수행한다.
 * - 목록/상세 조회와 생성/삭제 SQL을 캡슐화해 서비스 계층에 제공한다.
 * - DB Row를 API DTO로 매핑하는 규칙을 한곳에서 관리한다.
 */
#pragma once

#include <drogon/orm/Row.h>

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

#include "board/model/BoardTypes.h"

namespace board
{

// 게시판 데이터에 대한 SQL 조회/저장/삭제를 담당하는 매퍼 계층.
// 서비스 계층은 SQL 문자열을 직접 다루지 않고 이 인터페이스만 사용한다.
class BoardMapper
{
  public:
    BoardMapper() = default;

    // 조건에 맞는 게시글 총 개수를 반환한다.
    // 페이지네이션 계산(totalPages) 전에 호출되는 COUNT 전용 쿼리다.
    std::uint64_t countPosts(
        const std::optional<std::string> &dbTypeFilter,
        const std::optional<std::string> &titleSearchKeyword) const;

    // 조건 + 페이지(limit/offset)에 맞는 게시글 목록을 조회한다.
    // 작성자 조인, 상태 필터, 정렬(created_at DESC)을 포함한 조회 결과를 반환한다.
    std::vector<BoardPostDTO> findPosts(
        const std::optional<std::string> &dbTypeFilter,
        const std::optional<std::string> &titleSearchKeyword,
        std::uint32_t limit,
        std::uint64_t offset) const;

    // 단일 게시글을 ID로 조회한다.
    // 레코드가 없으면 nullopt를 반환한다.
    std::optional<BoardPostDTO> findById(std::uint64_t postId) const;

    // 새 게시글을 저장하고, 저장된 최종 레코드를 반환한다.
    // INSERT 직후 PK 재조회로 DB 기본값까지 반영된 레코드를 돌려준다.
    std::optional<BoardPostDTO> insertPost(
        std::uint64_t memberId,
        const CreateBoardPostRequestDTO &request) const;

    // 게시글을 물리 삭제한다.
    // 반환값은 affected rows > 0 여부다.
    bool deletePost(std::uint64_t postId) const;

  private:
    // DB 행(Row)을 API 전달용 DTO로 변환한다.
    // nullable/enum 컬럼을 클라이언트 계약 형식(optional/소문자 문자열)으로 정규화한다.
    static BoardPostDTO rowToBoardPostDTO(const drogon::orm::Row &row);
    // DB 저장값(post_type)을 클라이언트 표현값으로 변환한다.
    static std::string toClientType(const std::string &dbType);
    // DB 저장값(status)을 클라이언트 표현값으로 변환한다.
    static std::string toClientStatus(const std::string &dbStatus);

    // 로컬 개발 환경에서 필요한 테이블을 최초 1회 보장한다.
    void ensureTablesForLocalDev() const;
};

}  // namespace board
