/*
 * 파일 역할:
 * - 게시글 목록 조회 유스케이스의 서비스 인터페이스를 정의한다.
 * - 입력 정규화/검증, 페이징 계산, 결과 조립 로직의 진입점을 제공한다.
 * - 실제 DB 조회는 BoardMapper에 위임한다.
 */
#pragma once

#include "board/mapper/BoardMapper.h"
#include "board/model/BoardTypes.h"

namespace board
{

// 게시글 조회 유스케이스를 처리하는 서비스.
class BoardService_Query
{
  public:
    // 외부에서 주입한 Mapper를 참조로 보관한다.
    // 서비스 생명주기 동안 동일 매퍼를 공유해 조회 규칙을 일관되게 유지한다.
    explicit BoardService_Query(BoardMapper &mapper) : mapper_(mapper) {}

    // 필터/검색/페이징 조건으로 게시글 목록을 조회한다.
    // 성공/실패 상태, 메시지, 데이터 목록과 pagination 메타를 함께 반환한다.
    BoardListResultDTO getPosts(const BoardListRequestDTO &request);

  private:
    // DB 접근을 담당하는 매퍼 참조.
    BoardMapper &mapper_;
};

}  // namespace board
