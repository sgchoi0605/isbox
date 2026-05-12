#pragma once

#include <optional>
#include <string>

#include "../mappers/BoardMapper.h"
#include "../models/BoardTypes.h"

namespace board
{

// 게시판 기능의 서비스 계층이다.
// 컨트롤러 입력을 검증하고 정규화한 뒤 DB 작업은 매퍼에 위임한다.
class BoardService
{
  public:
    BoardService() = default;

    // 게시글 목록을 조회한다.
    // 필터값이 비어 있거나 "all"이면 전체 게시글을 반환한다.
    // 필터값이 "share" 또는 "exchange"이면 해당 유형만 반환한다.
    BoardListResultDTO getPosts(const std::optional<std::string> &typeFilter);

    // 새 게시글을 생성한다.
    // 유형/제목/내용/위치를 검증하고 유형 값을 DB 열거형 형식으로 변환한다.
    BoardCreateResultDTO createPost(std::uint64_t memberId,
                                    const CreateBoardPostRequestDTO &request);

    // 게시글 상태를 바꿔 소프트 삭제한다.
    // 게시글이 존재하는지, 요청자가 작성자인지 확인한다.
    BoardDeleteResultDTO deletePost(std::uint64_t memberId, std::uint64_t postId);

  private:
    // 입력값 앞뒤 공백을 제거한다.
    static std::string trim(std::string value);

    // 대소문자 구분 없는 비교를 위해 문자열을 소문자로 변환한다.
    static std::string toLower(std::string value);

    // 클라이언트가 보낸 게시글 유형이 지원되는 값인지 확인한다.
    static bool isAllowedType(const std::string &value);

    // 클라이언트 유형("share"/"exchange")을 DB 열거형 값으로 변환한다.
    static std::string toDbType(const std::string &value);

    // 게시글 저장/조회에 사용하는 데이터 접근 객체다.
    BoardMapper mapper_;
};

}  // 게시판 네임스페이스
