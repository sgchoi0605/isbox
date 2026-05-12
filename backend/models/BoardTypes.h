#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace board
{

// 게시글 생성 요청 본문(JSON)을 받기 위한 DTO.
class CreateBoardPostRequestDTO
{
  public:
    // 게시글 유형(예: share, exchange).
    std::string type;

    // 게시글 제목.
    std::string title;

    // 게시글 본문 내용.
    std::string content;

    // 거래/나눔 위치(선택값).
    std::optional<std::string> location;
};

// 게시글 단건 응답에 사용하는 DTO.
class BoardPostDTO
{
  public:
    // 게시글 PK.
    std::uint64_t postId{0};

    // 작성자 member_id.
    std::uint64_t memberId{0};

    // 게시글 유형.
    std::string type;

    // 게시글 제목/본문.
    std::string title;
    std::string content;

    // 위치 정보(없으면 nullopt).
    std::optional<std::string> location;

    // JOIN으로 조회한 작성자 이름.
    std::string authorName;

    // 게시글 상태(예: AVAILABLE, CLOSED).
    std::string status;

    // 생성 시각(문자열 포맷).
    std::string createdAt;
};

// 게시글 목록 조회 서비스 결과 래퍼 DTO.
class BoardListResultDTO
{
  public:
    bool ok{false};
    int statusCode{400};
    std::string message;
    std::vector<BoardPostDTO> posts;
};

// 게시글 생성 서비스 결과 래퍼 DTO.
class BoardCreateResultDTO
{
  public:
    bool ok{false};
    int statusCode{400};
    std::string message;
    std::optional<BoardPostDTO> post;
};

// 게시글 삭제 서비스 결과 래퍼 DTO.
class BoardDeleteResultDTO
{
  public:
    bool ok{false};
    int statusCode{400};
    std::string message;
};

}  // namespace board
