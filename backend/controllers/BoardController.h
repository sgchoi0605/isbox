#pragma once

#include <drogon/HttpTypes.h>
#include <drogon/drogon.h>

#include <cstdint>
#include <functional>
#include <optional>
#include <string>

#include "../services/BoardService.h"

namespace board
{

class BoardController
{
  public:
    BoardController() = default;

    void registerHandlers();

  private:
    using Callback = std::function<void(const drogon::HttpResponsePtr &)>;

    static Json::Value buildPostJson(const BoardPostDTO &post);
    static Json::Value buildPaginationJson(const BoardPaginationDTO &pagination);

    static void applyCors(const drogon::HttpRequestPtr &request,
                          const drogon::HttpResponsePtr &response);

    static std::optional<std::uint64_t> extractMemberIdHeader(
        const drogon::HttpRequestPtr &request);

    static std::optional<std::uint64_t> extractPostIdParameter(
        const drogon::HttpRequestPtr &request);

    static std::optional<std::uint32_t> parsePositiveIntegerParameter(
        const std::string &value,
        std::uint32_t minValue,
        std::uint32_t maxValue);

    void handleListPosts(const drogon::HttpRequestPtr &request, Callback &&callback);
    void handleCreatePost(const drogon::HttpRequestPtr &request, Callback &&callback);
    void handleDeletePost(const drogon::HttpRequestPtr &request, Callback &&callback);

    BoardService boardService_;
};

}  // namespace board
