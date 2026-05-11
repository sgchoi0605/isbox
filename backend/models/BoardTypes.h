#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace board
{

// 게시글 생성 요청에 사용하는 DTO.
// 컨트롤러가 JSON 본문을 이 타입으로 옮기고, 서비스가 필수값/길이/type을 검증한다.
class CreateBoardPostRequestDTO
{
public:
    // 클라이언트 입력값은 share/exchange 같은 소문자 문자열을 기대한다.
    std::string type;

    // 게시글 제목. 서비스에서 공백 제거 후 최대 길이를 검사한다.
    std::string title;

    // 게시글 본문. 공백만 있는 값은 서비스에서 빈 값으로 처리한다.
    std::string content;

    // 거래/나눔 위치는 선택값이며, 빈 문자열이면 값이 없는 것으로 정리된다.
    std::optional<std::string> location;
};

// 클라이언트 응답에 노출되는 게시글 DTO.
// BoardMapper가 DB 조회 결과를 이 타입으로 변환하고, BoardController가 JSON으로 직렬화한다.
class BoardPostDTO
{
public:
    // board_posts.post_id
    std::uint64_t postId{0};

    // 작성자 식별자. 삭제 권한 확인에도 사용한다.
    std::uint64_t memberId{0};

    // 프론트엔드가 사용하는 게시글 유형 문자열(share/exchange).
    std::string type;

    std::string title;
    std::string content;

    // DB의 빈 location은 nullopt로 표현해 JSON 응답에서 생략할 수 있게 한다.
    std::optional<std::string> location;

    // members 테이블과 조인해서 가져온 작성자 이름.
    std::string authorName;

    // 프론트엔드가 사용하는 게시글 상태 문자열(available/closed/deleted).
    std::string status;

    // 화면 표시용으로 포맷된 생성 시각 문자열.
    std::string createdAt;
};

// 게시글 목록 조회 결과를 담는 서비스 래퍼 DTO.
// 서비스는 성공 여부, HTTP 상태 코드, 사용자 메시지, 목록 데이터를 함께 반환한다.
class BoardListResultDTO
{
public:
    bool ok{false};
    int statusCode{400};
    std::string message;
    std::vector<BoardPostDTO> posts;
};

// 게시글 생성 결과를 담는 서비스 래퍼 DTO.
// 생성 성공 시 post에 방금 저장된 게시글 DTO가 들어간다.
class BoardCreateResultDTO
{
public:
    bool ok{false};
    int statusCode{400};
    std::string message;
    std::optional<BoardPostDTO> post;
};

// 게시글 삭제 결과를 담는 서비스 래퍼 DTO.
// 삭제 API는 본문 데이터 없이 처리 결과와 메시지만 내려준다.
class BoardDeleteResultDTO
{
public:
    bool ok{false};
    int statusCode{400};
    std::string message;
};

}  // namespace board
