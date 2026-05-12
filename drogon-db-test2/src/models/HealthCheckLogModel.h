#pragma once
// health_check_logs 테이블의 한 행(row)을 표현하는 ORM 모델.

#include <drogon/orm/Row.h>
// DB Row -> C++ 객체 변환에 사용.

#include <drogon/orm/SqlBinder.h>
// INSERT 바인딩 파라미터 구성에 사용.

#include <cstdint>
// PK 타입(std::uint64_t) 정의에 사용.

#include <stdexcept>
// 잘못된 컬럼 인덱스 처리 예외에 사용.

#include <string>
// note 문자열 필드 표현.

#include <utility>
// std::move 사용.

#include <vector>
// updateColumns() 반환 타입.

namespace model
{

class HealthCheckLogModel final
{
  public:
    using PrimaryKeyType = std::uint64_t;
    // health_check_logs.id(BIGINT AUTO_INCREMENT)의 C++ 타입.

    inline static const std::string tableName = "health_check_logs";
    // 매핑 대상 테이블명.

    inline static const std::string primaryKeyName = "id";
    // PK 컬럼명.

    explicit HealthCheckLogModel(std::string note)
        : id_(0), note_(std::move(note))
    {
        // 새 로그 생성용 생성자. id는 DB가 자동 발급한다.
    }

    explicit HealthCheckLogModel(const drogon::orm::Row &row)
        : id_(row["id"].as<std::uint64_t>()),
          note_(row["note"].as<std::string>())
    {
        // 조회한 DB Row를 모델로 복원.
    }

    static std::string sqlForFindingByPrimaryKey()
    {
        // PK 단건 조회 SQL.
        return "select `id`, `note` from `health_check_logs` where `id` = ?";
    }

    static std::string getColumnName(size_t index)
    {
        // Mapper가 컬럼 인덱스를 이름으로 바꿀 때 사용.
        switch (index)
        {
            case 0:
                return "id";
            case 1:
                return "note";
            default:
                throw std::out_of_range("HealthCheckLogModel column index is out of range");
        }
    }

    std::string sqlForInserting(bool &needSelection) const
    {
        // 로그 입력용 INSERT SQL.
        needSelection = false;
        return "insert into `health_check_logs` (`note`) values (?)";
    }

    void outputArgs(drogon::orm::internal::SqlBinder &binder) const
    {
        // INSERT SQL 바인딩 값(note).
        binder << note_;
    }

    std::vector<std::string> updateColumns() const
    {
        // 로그는 수정하지 않는 정책이라 UPDATE 대상 컬럼 없음.
        return {};
    }

    void updateArgs(drogon::orm::internal::SqlBinder &) const
    {
        // updateColumns()가 비어 있으므로 바인딩할 값이 없다.
    }

    std::uint64_t getPrimaryKey() const noexcept
    {
        return id_;
    }

    void updateId(std::uint64_t id) noexcept
    {
        // INSERT 후 DB가 생성한 id를 모델에 반영.
        id_ = id;
    }

    std::uint64_t id() const noexcept
    {
        return id_;
    }

    const std::string &note() const noexcept
    {
        return note_;
    }

  private:
    std::uint64_t id_;
    // DB AUTO_INCREMENT PK.

    std::string note_;
    // 로그 메시지.
};

}  // namespace model
