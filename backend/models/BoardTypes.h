#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace board
{

class CreateBoardPostRequestDTO
{
  public:
    std::string type;
    std::string title;
    std::string content;
    std::optional<std::string> location;
};

class BoardListRequestDTO
{
  public:
    std::string type{"all"};
    std::string search;
    std::uint32_t page{1};
    std::uint32_t size{9};
};

class BoardPostDTO
{
  public:
    std::uint64_t postId{0};
    std::uint64_t memberId{0};
    std::string type;
    std::string title;
    std::string content;
    std::optional<std::string> location;
    std::string authorName;
    std::string status;
    std::string createdAt;
};

class BoardPaginationDTO
{
  public:
    std::uint32_t page{1};
    std::uint32_t size{9};
    std::uint64_t totalItems{0};
    std::uint32_t totalPages{0};
    bool hasPrev{false};
    bool hasNext{false};
};

class BoardListResultDTO
{
  public:
    bool ok{false};
    int statusCode{400};
    std::string message;
    std::vector<BoardPostDTO> posts;
    BoardPaginationDTO pagination;
};

class BoardCreateResultDTO
{
  public:
    bool ok{false};
    int statusCode{400};
    std::string message;
    std::optional<BoardPostDTO> post;
};

class BoardDeleteResultDTO
{
  public:
    bool ok{false};
    int statusCode{400};
    std::string message;
};

}  // namespace board
