#pragma once
// 하나의 번역 단위에서 헤더가 중복 포함되는 것을 방지한다.

#include <drogon/orm/Row.h>
// Row는 SQL 결과의 한 행을 표현하는 Drogon 타입이다.

#include <stdexcept>
// 잘못된 컬럼 인덱스 접근 시 std::out_of_range 예외를 사용한다.

#include <string>
// 테이블 메타데이터와 모델 필드 문자열 표현에 사용한다.

namespace model
{
// 데모 모델 타입을 전용 네임스페이스로 분리한다.

class HealthCheckModel final
{
  public:
    using PrimaryKeyType = int;
    // Mapper가 기본키 C++ 타입을 알 수 있도록 별칭을 제공한다.

    inline static const std::string tableName = "health_checks";
    // Mapper 내부의 공통 SQL 생성 로직에서 테이블 이름으로 사용한다.

    inline static const std::string primaryKeyName = "id";
    // 매핑된 테이블의 기본키 컬럼명을 나타낸다.

    static std::string sqlForFindingByPrimaryKey()
    {
        // Mapper::findByPrimaryKey()가 사용할 PK 조회 SQL을 반환한다.
        return "select `id`, `ok`, `note` from `health_checks` where `id` = ?";
    }

    static std::string getColumnName(size_t index)
    {
        // Mapper::orderBy(index)에서 인덱스 기반 컬럼명을 얻을 때 사용한다.
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

    explicit HealthCheckModel(const drogon::orm::Row &row)
        : id_(row["id"].as<int>()),
          ok_(row["ok"].as<int>()),
          note_(row["note"].as<std::string>())
    {
        // SQL 결과 한 행을 강타입 C++ 모델 객체로 변환한다.
    }

    int id() const noexcept
    {
        // 기본키 값을 반환한다.
        return id_;
    }

    int ok() const noexcept
    {
        // 상태값(정수)을 반환한다.
        return ok_;
    }

    const std::string &note() const noexcept
    {
        // 설명 문자열을 반환한다.
        return note_;
    }

  private:
    int id_;
    // 기본키 값.

    int ok_;
    // 기존 프로젝트 응답과 호환되는 상태 숫자값.

    std::string note_;
    // 사람이 읽을 수 있는 상태 설명 문자열.
};

}  // namespace model
