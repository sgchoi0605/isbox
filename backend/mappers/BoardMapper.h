#pragma once

#include <drogon/orm/Row.h>

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

#include "../models/BoardTypes.h"

namespace board
{

class BoardMapper
{
  public:
    BoardMapper() = default;

    std::uint64_t countPosts(
        const std::optional<std::string> &dbTypeFilter,
        const std::optional<std::string> &titleSearchKeyword) const;

    std::vector<BoardPostDTO> findPosts(
        const std::optional<std::string> &dbTypeFilter,
        const std::optional<std::string> &titleSearchKeyword,
        std::uint32_t limit,
        std::uint64_t offset) const;

    std::optional<BoardPostDTO> findById(std::uint64_t postId) const;

    std::optional<BoardPostDTO> insertPost(
        std::uint64_t memberId,
        const CreateBoardPostRequestDTO &request) const;

    void markPostDeleted(std::uint64_t postId) const;

  private:
    static BoardPostDTO rowToBoardPostDTO(const drogon::orm::Row &row);
    static std::string toClientType(const std::string &dbType);
    static std::string toClientStatus(const std::string &dbStatus);

    void ensureTablesForLocalDev() const;
};

}  // namespace board
