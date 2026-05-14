/*
 * 파일 역할:
 * - `board_posts` 테이블과 대응되는 Drogon ORM 모델을 정의한다.
 * - Mapper가 사용하는 INSERT/조회 시 SQL 메타 규약을 제공한다.
 * - DB Row와 C++ 객체 간 변환 책임을 모델 수준에서 캡슐화한다.
 */
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

// board_posts 테이블 한 행을 표현하는 Drogon ORM 모델.
// BoardMapper가 insert/find 작업에서 직접 사용하는 경량 모델이다.
class BoardPostModel final
{
  public:
    // board_posts.post_id(BIGINT)와 매칭되는 C++ PK 타입.
    using PrimaryKeyType = std::uint64_t;

    // Drogon Mapper가 참조하는 테이블/PK 메타데이터.
    inline static const std::string tableName = "board_posts";
    inline static const std::string primaryKeyName = "post_id";

    // 새 게시글 삽입용 생성자(INSERT 전 PK는 0).
    // PK는 DB auto increment가 부여하므로 초기값을 0으로 유지한다.
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

    // DB 조회 Row를 모델로 복원하는 생성자.
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

    // PK 기반 단건 조회 SQL.
    // NULL 가능 컬럼(location/created_at)은 COALESCE로 빈 문자열로 정규화한다.
    static std::string sqlForFindingByPrimaryKey()
    {
        return "SELECT post_id, member_id, post_type, title, content, "
               "COALESCE(location, '') AS location, status, "
               "COALESCE(DATE_FORMAT(created_at, '%Y-%m-%d %H:%i:%s'), '') AS "
               "created_at "
               "FROM board_posts WHERE post_id = ?";
    }

    // Drogon Mapper가 인덱스를 실제 컬럼명으로 변환할 때 사용한다.
    // 인덱스 순서는 SELECT 컬럼 순서와 반드시 일치해야 한다.
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
    // needSelection=false: insert 직후 Mapper 추가 조회는 수행하지 않는다.
    // 필요한 경우 상위 계층에서 PK 기반 재조회(findById)를 명시적으로 수행한다.
    std::string sqlForInserting(bool &needSelection) const
    {
        needSelection = false;
        return "INSERT INTO board_posts "
               "(member_id, post_type, title, content, location, status) "
               "VALUES (?, ?, ?, ?, ?, ?)";
    }

    // INSERT SQL의 바인딩 인자를 순서대로 채운다.
    void outputArgs(drogon::orm::internal::SqlBinder &binder) const
    {
        binder << memberId_ << postType_ << title_ << content_ << location_
               << status_;
    }

    // 현재 모델은 UPDATE 경로를 사용하지 않으므로 대상 컬럼이 없다.
    std::vector<std::string> updateColumns() const
    {
        return {};
    }

    // updateColumns가 비어 있어 실제 바인딩할 인자도 없다.
    void updateArgs(drogon::orm::internal::SqlBinder &) const
    {
    }

    // Mapper가 참조할 현재 PK 값.
    std::uint64_t getPrimaryKey() const noexcept
    {
        return postId_;
    }

    // INSERT 후 DB가 부여한 PK를 모델에 반영한다.
    void updateId(std::uint64_t id) noexcept
    {
        postId_ = id;
    }

    // 아래 getter는 서비스/매퍼 계층에서 모델 값을 읽을 때 사용한다.
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

    // 내부 저장은 문자열이지만 외부에는 optional로 노출한다.
    // DB NULL/빈문자열 처리 차이를 상위 계층으로 전파하지 않기 위한 어댑터다.
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
    // board_posts 컬럼에 대응되는 모델 필드.
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
