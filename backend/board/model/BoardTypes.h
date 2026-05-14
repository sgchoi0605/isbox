/*
 * 파일 역할:
 * - 게시판 계층 간에 전달되는 DTO(요청/응답/페이징) 타입을 정의한다.
 * - 컨트롤러/서비스/매퍼가 공유하는 데이터 계약을 한곳에서 고정한다.
 * - API 결과의 공통 필드(ok/statusCode/message) 구조를 표준화한다.
 */
#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace board
{

// 게시글 생성 요청 DTO.
// 컨트롤러 JSON 본문을 서비스 계층 입력으로 전달할 때 사용된다.
class CreateBoardPostRequestDTO
{
  public:
    // 게시글 유형(share/exchange).
    std::string type;
    // 게시글 제목.
    std::string title;
    // 게시글 본문.
    std::string content;
    // 선택 위치 정보.
    std::optional<std::string> location;
};

// 게시글 목록 조회 요청 DTO.
// 쿼리 파라미터(type/search/page/size)를 서비스 입력 모델로 캡슐화한다.
class BoardListRequestDTO
{
  public:
    // 필터 유형(all/share/exchange).
    std::string type{"all"};
    // 제목 검색어.
    std::string search;
    // 요청 페이지(1-base).
    std::uint32_t page{1};
    // 페이지 크기.
    std::uint32_t size{9};
};

// 단일 게시글 응답 DTO.
// 목록 조회/생성 응답에서 공통으로 재사용되는 게시글 표현이다.
class BoardPostDTO
{
  public:
    // 게시글 ID.
    std::uint64_t postId{0};
    // 작성자 회원 ID.
    std::uint64_t memberId{0};
    // 게시글 유형.
    std::string type;
    // 제목.
    std::string title;
    // 내용.
    std::string content;
    // 선택 위치 정보.
    std::optional<std::string> location;
    // 작성자 표시 이름.
    std::string authorName;
    // 상태(available/closed/deleted).
    std::string status;
    // 생성 시각 문자열.
    std::string createdAt;
};

// 목록 응답용 페이징 메타 DTO.
// 클라이언트 페이지 네비게이션 계산에 필요한 메타 정보를 담는다.
class BoardPaginationDTO
{
  public:
    // 현재 페이지(1-base).
    std::uint32_t page{1};
    // 페이지 크기.
    std::uint32_t size{9};
    // 전체 게시글 수.
    std::uint64_t totalItems{0};
    // 전체 페이지 수.
    std::uint32_t totalPages{0};
    // 이전 페이지 존재 여부.
    bool hasPrev{false};
    // 다음 페이지 존재 여부.
    bool hasNext{false};
};

// 게시글 목록 API 결과 DTO.
// 게시글 배열과 페이징 정보를 함께 전달한다.
class BoardListResultDTO
{
  public:
    // 처리 성공 여부.
    bool ok{false};
    // HTTP 상태 코드.
    int statusCode{400};
    // 결과 메시지.
    std::string message;
    // 게시글 목록 데이터.
    std::vector<BoardPostDTO> posts;
    // 페이징 정보.
    BoardPaginationDTO pagination;
};

// 게시글 생성 API 결과 DTO.
// 생성 성공 시 post 필드에 새 게시글 스냅샷을 담는다.
class BoardCreateResultDTO
{
  public:
    // 처리 성공 여부.
    bool ok{false};
    // HTTP 상태 코드.
    int statusCode{400};
    // 결과 메시지.
    std::string message;
    // 생성된 게시글(성공 시).
    std::optional<BoardPostDTO> post;
};

// 게시글 삭제 API 결과 DTO.
// 삭제 요청은 본문 데이터 없이 상태와 메시지로 결과를 전달한다.
class BoardDeleteResultDTO
{
  public:
    // 처리 성공 여부.
    bool ok{false};
    // HTTP 상태 코드.
    int statusCode{400};
    // 결과 메시지.
    std::string message;
};

}  // namespace board
