#pragma once

#include <optional>
#include <string>

#include "../mappers/BoardMapper.h"
#include "../models/BoardTypes.h"

namespace board
{

// 게시판 비즈니스 계층이다.
// 컨트롤러에서 받은 DTO를 검증/정규화한 뒤 Mapper를 통해 DB 작업을 수행한다.
class BoardService
{
  public:
    BoardService() = default;

    // 게시글 목록을 조회한다.
    // typeFilter가 비어 있거나 all이면 전체를, share/exchange면 해당 유형만 반환한다.
    BoardListResultDTO getPosts(const std::optional<std::string> &typeFilter);

    // 새 게시글을 생성한다.
    // 제목/내용/위치 길이와 게시글 유형을 검증한 뒤 DB 저장용 값으로 변환한다.
    BoardCreateResultDTO createPost(std::uint64_t memberId,
                                    const CreateBoardPostRequestDTO &request);

    // 게시글을 삭제 상태로 변경한다.
    // 실제 삭제 전에 게시글 존재 여부와 작성자 소유권을 확인한다.
    BoardDeleteResultDTO deletePost(std::uint64_t memberId, std::uint64_t postId);

  private:
    // 사용자 입력 앞뒤 공백을 제거해 빈 값 검증을 일관되게 만든다.
    static std::string trim(std::string value);

    // 클라이언트가 보낸 type 값을 대소문자와 무관하게 비교하기 위해 소문자로 맞춘다.
    static std::string toLower(std::string value);

    // 현재 게시판에서 허용하는 게시글 유형인지 확인한다.
    static bool isAllowedType(const std::string &value);

    // 클라이언트 유형값을 DB enum 값(SHARE/EXCHANGE)으로 변환한다.
    static std::string toDbType(const std::string &value);

    BoardMapper mapper_;
};

}  // namespace board
