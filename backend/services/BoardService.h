#pragma once

#include <optional>
#include <string>

#include "../mappers/BoardMapper.h"
#include "../models/BoardTypes.h"

namespace board
{

class BoardService
{
  public:
    BoardService() = default;

    BoardListResultDTO getPosts(const BoardListRequestDTO &request);

    BoardCreateResultDTO createPost(std::uint64_t memberId,
                                    const CreateBoardPostRequestDTO &request);

    BoardDeleteResultDTO deletePost(std::uint64_t memberId, std::uint64_t postId);

  private:
    static std::string trim(std::string value);
    static std::string toLower(std::string value);
    static bool isAllowedType(const std::string &value);
    static std::string toDbType(const std::string &value);

    BoardMapper mapper_;
};

}  // namespace board
