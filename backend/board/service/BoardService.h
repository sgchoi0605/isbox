/*
 * 파일 역할:
 * - 게시판 서비스의 파사드(Facade) 인터페이스를 정의한다.
 * - 조회(Query)와 변경(Command) 하위 서비스를 조합해 단일 진입점을 제공한다.
 * - 컨트롤러가 내부 분리 구조를 몰라도 기능을 호출할 수 있게 만든다.
 */
#pragma once

#include <cstdint>

#include "board/model/BoardTypes.h"
#include "board/service/BoardService_Command.h"
#include "board/service/BoardService_Query.h"

namespace board
{

// 게시판 서비스 파사드.
// 읽기/쓰기 책임을 하위 서비스(Query/Command)로 위임한다.
class BoardService
{
  public:
    // 동일한 Mapper 인스턴스를 Query/Command에 공유한다.
    // 조회/변경 모두 같은 DB 접근 규칙을 따르도록 보장한다.
    BoardService();

    // 게시글 목록 조회 요청을 처리한다.
    // 내부적으로 BoardService_Query에 위임한다.
    BoardListResultDTO getPosts(const BoardListRequestDTO &request);
    // 게시글 생성 요청을 처리한다.
    // 내부적으로 BoardService_Command에 위임한다.
    BoardCreateResultDTO createPost(std::uint64_t memberId,
                                    const CreateBoardPostRequestDTO &request);
    // 게시글 삭제 요청을 처리한다.
    // 내부적으로 BoardService_Command에 위임한다.
    BoardDeleteResultDTO deletePost(std::uint64_t memberId, std::uint64_t postId);

  private:
    // 데이터 접근 매퍼(공유).
    BoardMapper mapper_;
    // 조회 전용 비즈니스 로직.
    BoardService_Query query_;
    // 변경(생성/삭제) 비즈니스 로직.
    BoardService_Command command_;
};

}  // namespace board
