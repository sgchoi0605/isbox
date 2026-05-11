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

// board_posts 테이블에 매핑되는 Drogon ORM 모델.
// Mapper 삽입에 필요한 SQL, 바인딩 값, 기본키 갱신 방식을 Drogon Mapper 규약에 맞춰 제공한다.
class BoardPostModel final
{
  public:
    // Drogon Mapper가 기본키 타입을 알 수 있도록 노출한다.
    using PrimaryKeyType = std::uint64_t;

    // ORM 삽입과 기본키 갱신에서 사용하는 테이블/기본키 이름이다.
    inline static const std::string tableName = "board_posts";
    inline static const std::string primaryKeyName = "post_id";

    // 새 게시글을 DB에 삽입할 때 사용하는 생성자다.
    // postId_는 DB 자동 증가 값으로 채워지기 전이라 0으로 시작한다.
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

    // Drogon Row에서 모델을 복원할 때 사용하는 생성자다.
    // 조회 SQL에서 COALESCE로 location/created_at을 빈 문자열로 보정한 값을 기대한다.
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

    // 기본키 조회용 SQL이다.
    // location과 created_at은 null 가능성이 있어 응답 변환이 편하도록 빈 문자열로 보정한다.
    static std::string sqlForFindingByPrimaryKey()
    {
        return "SELECT post_id, member_id, post_type, title, content, "
               "COALESCE(location, '') AS location, status, "
               "COALESCE(DATE_FORMAT(created_at, '%Y-%m-%d %H:%i:%s'), '') AS "
               "created_at "
               "FROM board_posts WHERE post_id = ?";
    }

    // Drogon Mapper가 컬럼 인덱스를 실제 DB 컬럼명으로 바꿀 때 사용한다.
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

    // 삽입 시 사용할 SQL을 반환한다.
    // 삽입 후 별도 SELECT가 필요 없도록 needSelection은 false로 둔다.
    std::string sqlForInserting(bool &needSelection) const
    {
        needSelection = false;
        return "INSERT INTO board_posts "
               "(member_id, post_type, title, content, location, status) "
               "VALUES (?, ?, ?, ?, ?, ?)";
    }

    // 삽입 SQL의 ? 위치에 들어갈 값을 순서대로 바인딩한다.
    void outputArgs(drogon::orm::internal::SqlBinder &binder) const
    {
        binder << memberId_ << postType_ << title_ << content_ << location_
               << status_;
    }

    // 현재 게시글 수정 기능은 없으므로 수정 대상 컬럼은 비워 둔다.
    std::vector<std::string> updateColumns() const
    {
        return {};
    }

    // updateColumns가 비어 있으므로 수정 바인딩 값도 없다.
    void updateArgs(drogon::orm::internal::SqlBinder &) const
    {
    }

    // Drogon Mapper가 삽입 이후 기본키를 읽을 때 사용하는 조회 함수다.
    std::uint64_t getPrimaryKey() const noexcept
    {
        return postId_;
    }

    // 삽입 성공 후 DB가 발급한 자동 증가 ID를 모델에 반영한다.
    void updateId(std::uint64_t id) noexcept
    {
        postId_ = id;
    }

    // 아래 조회 함수들은 서비스/매퍼가 필요한 값만 읽을 수 있게 모델 내부 필드를 감싼다.
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
    // board_posts 테이블 컬럼을 그대로 보관한다.
    // location_은 DB에는 빈 문자열로 들어갈 수 있지만, 공개 조회 함수에서는 선택값으로 바꿔 반환한다.
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
