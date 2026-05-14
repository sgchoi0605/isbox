/*
 * 파일 역할:
 * - 게시글 목록 조회 유스케이스의 상세 비즈니스 로직을 구현한다.
 * - 필터/검색어 정규화, 페이지 보정, pagination 메타 계산을 수행한다.
 * - DB 예외를 API 응답용 상태코드/메시지로 변환한다.
 */
#include "board/service/BoardService_Query.h"

#include <drogon/orm/Exception.h>

#include <optional>

#include "board/service/BoardService_Validation.h"

namespace board
{

BoardListResultDTO BoardService_Query::getPosts(const BoardListRequestDTO &request)
{
    // 기본 실패 상태에서 검증 통과 시 성공값으로 덮어쓴다.
    // 단계별 early return을 사용해 실패 이유를 즉시 반환한다.
    BoardListResultDTO result;

    if (request.page == 0)
    {
        // page는 1 이상이어야 한다.
        result.statusCode = 400;
        result.message = "Invalid page parameter.";
        return result;
    }

    if (request.size == 0 || request.size > 50)
    {
        // size는 1~50 범위만 허용한다.
        result.statusCode = 400;
        result.message = "Invalid size parameter.";
        return result;
    }

    // type 필터를 정규화하고 DB 포맷으로 변환한다.
    // 빈 문자열 또는 "all"은 필터 미적용으로 해석한다.
    std::optional<std::string> dbTypeFilter;
    const auto normalizedType =
        BoardService_Validation::toLower(BoardService_Validation::trim(request.type));
    if (!normalizedType.empty() && normalizedType != "all")
    {
        if (!BoardService_Validation::isAllowedType(normalizedType))
        {
            // 허용 타입이 아니면 조회를 중단한다.
            result.statusCode = 400;
            result.message = "Invalid post type filter.";
            return result;
        }
        dbTypeFilter = BoardService_Validation::toDbType(normalizedType);
    }

    // 검색어는 공백 제거 후 비어있지 않을 때만 적용한다.
    // 빈 검색어에 대한 불필요한 LIKE 조회를 방지한다.
    std::optional<std::string> searchKeyword;
    const auto normalizedSearch = BoardService_Validation::trim(request.search);
    if (!normalizedSearch.empty())
    {
        searchKeyword = normalizedSearch;
    }

    // 응답 페이징 기본값을 요청값으로 먼저 채운다.
    // 이후 totalPages 계산 결과에 따라 page를 재보정할 수 있다.
    result.pagination.page = request.page;
    result.pagination.size = request.size;

    try
    {
        // 전체 건수를 먼저 조회해 페이지 정보를 계산한다.
        // 목록 조회와 동일 필터를 사용해 total count 불일치를 방지한다.
        const auto totalItems = mapper_.countPosts(dbTypeFilter, searchKeyword);
        result.pagination.totalItems = totalItems;

        // 올림 나눗셈으로 전체 페이지 수를 계산한다.
        const auto totalPages =
            totalItems == 0
                ? 0U
                : static_cast<std::uint32_t>(
                      (totalItems + static_cast<std::uint64_t>(request.size) - 1ULL) /
                      static_cast<std::uint64_t>(request.size));
        result.pagination.totalPages = totalPages;

        std::uint32_t resolvedPage = request.page;
        if (totalPages == 0)
        {
            // 데이터가 없을 때는 page를 1로 정규화한다.
            // 빈 목록에서도 프런트 페이지네이터가 안전하게 동작하도록 맞춘다.
            resolvedPage = 1;
        }
        else if (resolvedPage > totalPages)
        {
            // 요청 page가 범위를 넘으면 마지막 페이지로 보정한다.
            resolvedPage = totalPages;
        }

        // 이전/다음 페이지 존재 여부를 계산한다.
        result.pagination.page = resolvedPage;
        result.pagination.hasPrev = totalPages > 0 && resolvedPage > 1;
        result.pagination.hasNext = totalPages > 0 && resolvedPage < totalPages;

        // 최종 페이지 기준 offset을 계산한다.
        // totalPages=0인 경우 offset은 항상 0으로 고정한다.
        const auto offset = totalPages == 0
                                ? 0ULL
                                : static_cast<std::uint64_t>(resolvedPage - 1U) *
                                      static_cast<std::uint64_t>(request.size);

        if (totalItems > 0)
        {
            // 실제 데이터가 있을 때만 목록 조회를 수행한다.
            // totalItems=0이면 DB SELECT를 생략해 불필요한 쿼리를 줄인다.
            result.posts = mapper_.findPosts(
                dbTypeFilter, searchKeyword, request.size, offset);
        }

        // 조회 성공 결과.
        result.ok = true;
        result.statusCode = 200;
        result.message = "Posts loaded.";
        return result;
    }
    catch (const drogon::orm::DrogonDbException &)
    {
        // DB 계층 예외는 서버 오류로 변환한다.
        result.statusCode = 500;
        result.message = "Database error while loading posts.";
        return result;
    }
    catch (const std::exception &)
    {
        // 그 외 예외도 서버 오류로 변환한다.
        result.statusCode = 500;
        result.message = "Server error while loading posts.";
        return result;
    }
}

}  // namespace board
