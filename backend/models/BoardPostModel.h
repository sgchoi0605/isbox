#pragma once

#include <drogon/orm/Row.h>
#include <drogon/orm/SqlBinder.h>

#include <cstdint>
#include <optional>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace board
{

// board_posts 테이블 한 행(row)을 표현하는 Drogon ORM 모델.
// Mapper에서 INSERT/SELECT/UPDATE 시 필요한 SQL 규약 메서드를 함께 제공한다.
class BoardPostModel final
{
  public:
    // board_posts.post_id(BIGINT)의 C++ 표현 타입.
    using PrimaryKeyType = std::uint64_t;

    // Drogon Mapper가 내부적으로 사용하는 테이블/PK 메타정보.
    inline static const std::string tableName = "board_posts";
    inline static const std::string primaryKeyName = "post_id";

    // 새 게시글 생성용 생성자.
    // postId_는 DB AUTO_INCREMENT가 채우므로 0으로 시작한다.
    BoardPostModel(std::uint64_t memberId,
                   std::string postType,
                   std::string title,
                   std::string content,
                   std::string location,
                   std::string status = "AVAILABLE")
        : postId_(0),
          memberId_(memberId),
          postType_(std::move(postType)),
          title_(std::move(title)),
          content_(std::move(content)),
          location_(std::move(location)),
          status_(std::move(status))
    {
    }

    // DB 조회 결과(Row)를 모델 객체로 복원하는 생성자.
    explicit BoardPostModel(const drogon::orm::Row &row)
        : postId_(row["post_id"].as<std::uint64_t>()),
          memberId_(row["member_id"].as<std::uint64_t>()),
          postType_(row["post_type"].as<std::string>()),
          title_(row["title"].as<std::string>()),
          content_(row["content"].as<std::string>()),
          location_(row["location"].as<std::string>()),
          status_(row["status"].as<std::string>()),
          createdAt_(row["created_at"].as<std::string>())
    {
    }

    // PK 기준 단건 조회 SQL.
    // location/created_at이 NULL일 수 있어 COALESCE로 빈 문자열로 정규화한다.
    static std::string sqlForFindingByPrimaryKey()
    {
        return "SELECT post_id, member_id, post_type, title, content, "
               "COALESCE(location, '') AS location, status, "
               "COALESCE(DATE_FORMAT(created_at, '%Y-%m-%d %H:%i:%s'), '') AS "
               "created_at "
               "FROM board_posts WHERE post_id = ?";
    }

    // Drogon Mapper가 컬럼 인덱스를 실제 컬럼명으로 해석할 때 사용한다.
    static std::string getColumnName(size_t index)
    {
        switch (index)
        {
            case 0:
                return "post_id";
            case 1:
                return "member_id";
            case 2:
                return "post_type";
            case 3:
                return "title";
            case 4:
                return "content";
            case 5:
                return "location";
            case 6:
                return "status";
            case 7:
                return "created_at";
            default:
                throw std::out_of_range("BoardPostModel column index out of range");
        }
    }

    // INSERT SQL 템플릿.
    // needSelection=false: INSERT 직후 추가 SELECT 없이 처리한다.
    std::string sqlForInserting(bool &needSelection) const
    {
        needSelection = false;
        return "INSERT INTO board_posts "
               "(member_id, post_type, title, content, location, status) "
               "VALUES (?, ?, ?, ?, ?, ?)";
    }

    // INSERT SQL의 ? 자리로 들어갈 값을 순서대로 바인딩한다.
    void outputArgs(drogon::orm::internal::SqlBinder &binder) const
    {
        binder << memberId_ << postType_ << title_ << content_ << location_
               << status_;
    }

    // 현재 게시글 수정 기능이 없으므로 UPDATE 대상 컬럼은 비워둔다.
    std::vector<std::string> updateColumns() const
    {
        return {};
    }

    // updateColumns()가 비어 있어 실제로 바인딩할 값이 없다.
    void updateArgs(drogon::orm::internal::SqlBinder &) const
    {
    }

    // 모델의 현재 PK 값을 Mapper에 제공한다.
    std::uint64_t getPrimaryKey() const noexcept
    {
        return postId_;
    }

    // INSERT 성공 후 DB가 발급한 PK를 모델에 반영한다.
    void updateId(std::uint64_t id) noexcept
    {
        postId_ = id;
    }

    // 아래 getter들은 서비스/매퍼 계층에서 필요한 필드를 읽을 때 사용한다.
    std::uint64_t postId() const noexcept
    {
        return postId_;
    }

    std::uint64_t memberId() const noexcept
    {
        return memberId_;
    }

    const std::string &postType() const noexcept
    {
        return postType_;
    }

    const std::string &title() const noexcept
    {
        return title_;
    }

    const std::string &content() const noexcept
    {
        return content_;
    }

    // 내부 저장은 문자열이지만, 외부에는 optional 형태로 노출한다.
    std::optional<std::string> location() const
    {
        if (location_.empty())
        {
            return std::nullopt;
        }
        return location_;
    }

    const std::string &status() const noexcept
    {
        return status_;
    }

    const std::string &createdAt() const noexcept
    {
        return createdAt_;
    }

  private:
    // board_posts 각 컬럼과 1:1 대응되는 저장 필드.
    std::uint64_t postId_;
    std::uint64_t memberId_;
    std::string postType_;
    std::string title_;
    std::string content_;
    std::string location_;
    std::string status_;
    std::string createdAt_;
};

}  // namespace board
