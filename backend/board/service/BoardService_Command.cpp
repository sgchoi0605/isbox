/*
 * 파일 역할:
 * - 게시글 생성/삭제 유스케이스의 상세 비즈니스 로직을 구현한다.
 * - 입력 정규화, 유효성 검사, 권한 검증을 수행한다.
 * - DB 예외를 API 응답 상태코드/메시지로 변환한다.
 */
#include "board/service/BoardService_Command.h"

#include <drogon/orm/Exception.h>

#include <optional>

#include "board/service/BoardService_Validation.h"

namespace board
{

BoardCreateResultDTO BoardService_Command::createPost(
    std::uint64_t memberId,
    const CreateBoardPostRequestDTO &request)
{
    // 기본 실패 상태에서 검증을 통과하면 성공값으로 덮어쓴다.
    // 단계별 early return을 사용해 실패 원인을 즉시 반환한다.
    BoardCreateResultDTO result;

    if (memberId == 0)
    {
        // 인증 정보가 없으면 생성 불가.
        result.statusCode = 401;
        result.message = "Unauthorized.";
        return result;
    }

    // 입력값을 trim/lower 정규화해 이후 검증 규칙을 단순화한다.
    // 예: " SHARE "와 "share"를 동일 값으로 취급한다.
    const auto normalizedType =
        BoardService_Validation::toLower(BoardService_Validation::trim(request.type));
    const auto normalizedTitle = BoardService_Validation::trim(request.title);
    const auto normalizedContent = BoardService_Validation::trim(request.content);

    std::optional<std::string> normalizedLocation;
    if (request.location.has_value())
    {
        // location은 값이 있고 공백 제거 후 비어있지 않을 때만 유지한다.
        // 빈 문자열은 미입력과 동일하게 취급한다.
        const auto location = BoardService_Validation::trim(*request.location);
        if (!location.empty())
        {
            normalizedLocation = location;
        }
    }

    // 허용되지 않은 type은 즉시 실패 처리한다.
    if (!BoardService_Validation::isAllowedType(normalizedType))
    {
        result.statusCode = 400;
        result.message = "Type must be share or exchange.";
        return result;
    }

    if (normalizedTitle.empty())
    {
        // 제목 필수 검증.
        result.statusCode = 400;
        result.message = "Title is required.";
        return result;
    }

    if (normalizedTitle.size() > 100U)
    {
        // 제목 길이 제한 검증.
        result.statusCode = 400;
        result.message = "Title must be 100 characters or fewer.";
        return result;
    }

    if (normalizedContent.empty())
    {
        // 본문 필수 검증.
        result.statusCode = 400;
        result.message = "Content is required.";
        return result;
    }

    if (normalizedLocation.has_value() && normalizedLocation->size() > 100U)
    {
        // 위치 길이 제한 검증.
        result.statusCode = 400;
        result.message = "Location must be 100 characters or fewer.";
        return result;
    }

    // DB 저장 포맷(type 대문자)으로 재구성한 요청 DTO를 만든다.
    // 검증된 값만 별도 DTO로 구성해 하위 계층 전달값을 명확히 고정한다.
    CreateBoardPostRequestDTO normalizedRequest;
    normalizedRequest.type = BoardService_Validation::toDbType(normalizedType);
    normalizedRequest.title = normalizedTitle;
    normalizedRequest.content = normalizedContent;
    normalizedRequest.location = normalizedLocation;

    try
    {
        // INSERT 후 조회 결과를 받아 최종 응답을 구성한다.
        // insertPost는 optional을 반환하므로 null 여부를 반드시 확인한다.
        const auto created = mapper_.insertPost(memberId, normalizedRequest);
        if (!created.has_value())
        {
            result.statusCode = 500;
            result.message = "Failed to create post.";
            return result;
        }

        // 생성 성공 결과.
        result.ok = true;
        result.statusCode = 201;
        result.message = "Post created.";
        result.post = created;
        return result;
    }
    catch (const drogon::orm::DrogonDbException &)
    {
        // DB 계층 예외는 서버 오류로 변환한다.
        result.statusCode = 500;
        result.message = "Database error while creating post.";
        return result;
    }
    catch (const std::exception &)
    {
        // 그 외 예외도 서버 오류로 변환한다.
        result.statusCode = 500;
        result.message = "Server error while creating post.";
        return result;
    }
}

BoardDeleteResultDTO BoardService_Command::deletePost(std::uint64_t memberId,
                                                      std::uint64_t postId)
{
    // 기본 실패 상태에서 검증/삭제 성공 시 성공값으로 덮어쓴다.
    BoardDeleteResultDTO result;

    if (memberId == 0)
    {
        // 인증 정보가 없으면 삭제 불가.
        result.statusCode = 401;
        result.message = "Unauthorized.";
        return result;
    }

    if (postId == 0)
    {
        // 삭제 대상 ID 누락 검증.
        result.statusCode = 400;
        result.message = "Post id is required.";
        return result;
    }

    try
    {
        // 먼저 대상 게시글 존재 여부와 상태를 확인한다.
        // 존재하지 않거나 이미 삭제된 글은 동일하게 404로 응답한다.
        const auto post = mapper_.findById(postId);
        if (!post.has_value() || post->status == "deleted")
        {
            result.statusCode = 404;
            result.message = "Post not found.";
            return result;
        }

        // 본인 글만 삭제 가능하다.
        // 권한 없는 삭제 요청은 403으로 분리해 전달한다.
        if (post->memberId != memberId)
        {
            result.statusCode = 403;
            result.message = "You can delete only your own post.";
            return result;
        }

        // 실제 DELETE 결과가 0행이면 없는 글로 처리한다.
        // 동시성 상황에서 직전 삭제가 발생한 케이스도 여기에 포함된다.
        if (!mapper_.deletePost(postId))
        {
            result.statusCode = 404;
            result.message = "Post not found.";
            return result;
        }

        // 삭제 성공 결과.
        result.ok = true;
        result.statusCode = 200;
        result.message = "Post deleted.";
        return result;
    }
    catch (const drogon::orm::DrogonDbException &)
    {
        // DB 계층 예외는 서버 오류로 변환한다.
        result.statusCode = 500;
        result.message = "Database error while deleting post.";
        return result;
    }
    catch (const std::exception &)
    {
        // 그 외 예외도 서버 오류로 변환한다.
        result.statusCode = 500;
        result.message = "Server error while deleting post.";
        return result;
    }
}

}  // namespace board
