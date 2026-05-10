#pragma once
// 하나의 번역 단위에서 헤더가 중복 포함되는 것을 막습니다.

#include <drogon/orm/Row.h>
// DB 조회 결과 한 줄을 C++ 객체로 바꿀 때 사용하는 Drogon Row 타입입니다.

#include <drogon/orm/SqlBinder.h>
// Mapper가 INSERT/UPDATE 파라미터를 바인딩할 때 사용하는 타입입니다.

#include <cstdint>
// 고정 크기 정수 타입을 사용하기 위해 포함합니다.

#include <stdexcept>
// 잘못된 컬럼 인덱스 접근 시 std::out_of_range 예외를 던지기 위해 사용합니다.

#include <string>
// note 같은 문자열 필드를 표현합니다.

#include <utility>
// 생성자에서 std::move로 문자열 소유권을 옮기기 위해 사용합니다.

#include <vector>
// Mapper::update()가 수정할 컬럼 목록을 std::vector로 돌려주기 위해 사용합니다.

namespace model
{
// DB 테이블을 C++ 타입으로 표현하는 모델들을 model namespace에 둡니다.

class HealthCheckModel final
{
  public:
    using PrimaryKeyType = int;
    // Drogon Mapper가 이 모델의 기본키 타입을 알 수 있게 하는 별칭입니다.

    inline static const std::string tableName = "health_checks";
    // 이 C++ 객체가 매핑되는 DB 테이블 이름입니다.

    inline static const std::string primaryKeyName = "id";
    // health_checks 테이블의 기본키 컬럼 이름입니다.

    HealthCheckModel(int id, int ok, std::string note)
        : id_(id), ok_(ok), note_(std::move(note))
    {
        // 새 row를 코드에서 직접 만들 때 사용하는 생성자입니다.
        // 예: HealthCheckModel row(1, 1, "success");
    }

    explicit HealthCheckModel(const drogon::orm::Row &row)
        : id_(row["id"].as<int>()),
          ok_(row["ok"].as<int>()),
          note_(row["note"].as<std::string>())
    {
        // DB에서 조회된 Row를 C++ 객체로 변환합니다.
        // 이 생성자가 있어서 Mapper<HealthCheckModel>이 조회 결과를 모델 객체로 만들어 줍니다.
    }

    static std::string sqlForFindingByPrimaryKey()
    {
        // Mapper::findByPrimaryKey()가 사용할 최소 SQL입니다.
        // row 조작은 Mapper가 맡지만, 기본키 조회 SQL은 모델이 자신의 테이블 구조를 알려줘야 합니다.
        return "select `id`, `ok`, `note` from `health_checks` where `id` = ?";
    }

    static std::string getColumnName(size_t index)
    {
        // Mapper::orderBy(index) 같은 인덱스 기반 API에서 컬럼명을 찾을 때 사용합니다.
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
        // Mapper::insert()가 사용할 INSERT 형태를 모델이 알려줍니다.
        // 값 바인딩은 아래 outputArgs()가 담당하므로 여기에는 실제 값이 들어가지 않습니다.
        needSelection = false;
        return "insert into `health_checks` (`id`, `ok`, `note`) values (?, ?, ?)";
    }

    void outputArgs(drogon::orm::internal::SqlBinder &binder) const
    {
        // INSERT의 ? 자리에 C++ 객체의 필드값을 순서대로 바인딩합니다.
        binder << id_ << ok_ << note_;
    }

    std::vector<std::string> updateColumns() const
    {
        // Mapper::update()가 수정할 컬럼 목록입니다. 기본키 id는 WHERE 조건으로 쓰이므로 제외합니다.
        return {"ok", "note"};
    }

    void updateArgs(drogon::orm::internal::SqlBinder &binder) const
    {
        // UPDATE의 SET 절에 들어갈 값을 컬럼 목록과 같은 순서로 바인딩합니다.
        binder << ok_ << note_;
    }

    int getPrimaryKey() const noexcept
    {
        // Mapper::update()가 WHERE id = ? 조건을 만들 때 사용하는 기본키 값입니다.
        return id_;
    }

    void updateId(std::uint64_t)
    {
        // 이 모델은 id를 코드에서 직접 정하는 테스트용 row입니다.
        // MySQL insertId가 0으로 돌아와도 기존 id를 유지해야 하므로 아무 것도 하지 않습니다.
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

  private:
    int id_;
    // 기본키입니다. 테스트에서는 항상 1번 row를 사용합니다.

    int ok_;
    // 성공 여부를 숫자로 저장합니다. 기존 응답의 value와 호환되도록 int를 사용합니다.

    std::string note_;
    // 사람이 읽을 수 있는 상태 메시지입니다.
};

}  // namespace model
