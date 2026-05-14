/*
 * 파일 역할:
 * - 게시글 변경 유스케이스(생성/삭제) 서비스 인터페이스를 정의한다.
 * - 입력 검증, 권한 확인, 저장/삭제 흐름의 진입점을 제공한다.
 * - 실제 DB 반영은 BoardMapper에 위임한다.
 */
#pragma once

#include <cstdint>

#include "board/mapper/BoardMapper.h"
#include "board/model/BoardTypes.h"

namespace board
{

// 게시글 변경(생성/삭제) 유스케이스를 처리하는 서비스.
class BoardService_Command
{
  public:
    // 외부에서 주입한 Mapper를 참조로 보관한다.
    // 조회/변경 서비스 모두 동일한 매퍼 인스턴스를 공유한다.
    explicit BoardService_Command(BoardMapper &mapper) : mapper_(mapper) {}

    // 게시글을 생성한다.
    // 타입/제목/본문/위치 검증을 통과하면 DB 저장을 수행한다.
    BoardCreateResultDTO createPost(std::uint64_t memberId,
                                    const CreateBoardPostRequestDTO &request);
    // 게시글을 삭제한다.
    // 요청자(memberId)와 작성자(memberId) 일치 여부를 확인한 뒤 삭제한다.
    BoardDeleteResultDTO deletePost(std::uint64_t memberId, std::uint64_t postId);

  private:
    // DB 접근을 담당하는 매퍼 참조.
    BoardMapper &mapper_;
};

}  // namespace board
