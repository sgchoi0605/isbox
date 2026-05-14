/*
 * 파일 역할:
 * - BoardService 파사드의 얇은 위임 로직을 구현한다.
 * - Query/Command 하위 서비스에 요청을 분기한다.
 * - 컨트롤러와 세부 비즈니스 구현 사이 결합도를 낮춘다.
 */
#include "board/service/BoardService.h"

namespace board
{

// Query/Command가 같은 Mapper를 참조하도록 생성 순서를 고정한다.
// 동일 트랜잭션 정책/매핑 규칙을 공유하기 위한 기본 구성이다.
BoardService::BoardService() : query_(mapper_), command_(mapper_) {}

BoardListResultDTO BoardService::getPosts(const BoardListRequestDTO &request)
{
    // 조회 요청은 Query 서비스로 위임한다.
    // 파사드는 전달/조합만 담당하고 조회 규칙 자체는 Query에 둔다.
    return query_.getPosts(request);
}

BoardCreateResultDTO BoardService::createPost(
    std::uint64_t memberId,
    const CreateBoardPostRequestDTO &request)
{
    // 생성 요청은 Command 서비스로 위임한다.
    // 입력 검증/저장/오류 매핑은 Command 계층에서 처리한다.
    return command_.createPost(memberId, request);
}

BoardDeleteResultDTO BoardService::deletePost(std::uint64_t memberId,
                                              std::uint64_t postId)
{
    // 삭제 요청은 Command 서비스로 위임한다.
    // 권한 검증과 실제 삭제 동작은 Command 계층이 책임진다.
    return command_.deletePost(memberId, postId);
}

}  // namespace board
