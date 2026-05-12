#pragma once
// 헤더 중복 포함 방지.

#include <drogon/orm/Row.h>
// DB 조회 결과 한 줄(Row)을 C++ 객체로 변환할 때 사용.

#include <drogon/orm/SqlBinder.h>
// INSERT/UPDATE 쿼리의 바인딩 파라미터를 채울 때 사용.

#include <cstdint>
// 고정 크기 정수 타입 사용.

#include <stdexcept>
// 잘못된 컬럼 인덱스 처리(out_of_range) 예외 사용.

#include <string>
// note 문자열 필드 표현.

#include <utility>
// std::move 사용.

#include <vector>
// updateColumns() 반환 타입.

namespace model
{

// health_checks 테이블 한 행을 표현하는 ORM 모델.
class HealthCheckModel final
{
  private:
    int id_;
    // PK. 테스트에서는 보통 1번 row를 사용.

    int ok_;
    // 상태 값(성공/실패)을 숫자로 표현.

    std::string note_;
    // 상태 설명 메시지.

  public:
    using PrimaryKeyType = int;
    // Drogon Mapper가 인식할 PK 타입.

    inline static const std::string tableName = "health_checks";
    // 매핑 대상 테이블명.

    inline static const std::string primaryKeyName = "id";
    // PK 컬럼명.

    HealthCheckModel(int id, int ok, std::string note)
        : id_(id), ok_(ok), note_(std::move(note))
    {
        // 코드에서 직접 모델을 만들 때 사용.
    }

    explicit HealthCheckModel(const drogon::orm::Row &row)
        : id_(row["id"].as<int>()),
          ok_(row["ok"].as<int>()),
          note_(row["note"].as<std::string>())
    {
        // DB Row를 모델 객체로 복원.
    }

    static std::string sqlForFindingByPrimaryKey()
    {
        // PK 단건 조회 SQL.
        return "select `id`, `ok`, `note` from `health_checks` where `id` = ?";
    }

    static std::string getColumnName(size_t index)
    {
        // Mapper가 컬럼 인덱스를 이름으로 해석할 때 사용.
        switch (index)
        {
            case 0:
                return "id";
            case 1:
                return "ok";
            case 2:
                return "note";
            default:
                throw std::out_of_range("HealthCheckModel column index is out of range");
        }
    }

    std::string sqlForInserting(bool &needSelection) const
    {
        // INSERT SQL 템플릿.
        // INSERT 직후 별도 SELECT가 필요 없어 false로 둔다.
        needSelection = false;
        return "insert into `health_checks` (`id`, `ok`, `note`) values (?, ?, ?)";
    }

    void outputArgs(drogon::orm::internal::SqlBinder &binder) const
    {
        // INSERT SQL의 ? 자리에 들어갈 값 바인딩.
        binder << id_ << ok_ << note_;
    }

    std::vector<std::string> updateColumns() const
    {
        // UPDATE 시 변경할 컬럼 목록.
        return {"ok", "note"};
    }

    void updateArgs(drogon::orm::internal::SqlBinder &binder) const
    {
        // updateColumns() 순서와 동일하게 값 바인딩.
        binder << ok_ << note_;
    }

    int getPrimaryKey() const noexcept
    {
        // UPDATE/조회에 사용할 현재 PK 값.
        return id_;
    }

    void updateId(std::uint64_t)
    {
        // 이 모델은 id를 코드에서 직접 지정하므로 INSERT 후 id 갱신 불필요.
    }

    int id() const noexcept
    {
        return id_;
    }

    int ok() const noexcept
    {
        return ok_;
    }

    const std::string &note() const noexcept
    {
        return note_;
    }
};

}  // namespace model
