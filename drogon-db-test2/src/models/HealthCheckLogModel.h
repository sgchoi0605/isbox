#pragma once
// health_check_logs 테이블 한 줄을 표현하는 ORM 모델입니다.

#include <drogon/orm/Row.h>
// DB Row를 C++ 모델로 바꾸기 위해 사용합니다.

#include <drogon/orm/SqlBinder.h>
// Mapper가 INSERT 파라미터를 바인딩할 때 사용합니다.

#include <cstdint>
// 자동 증가 id를 담을 정수 타입에 사용합니다.

#include <stdexcept>
// 잘못된 컬럼 인덱스 접근 시 예외를 던지기 위해 사용합니다.

#include <string>
// 로그 메시지와 생성 시각 문자열을 저장합니다.

#include <utility>
// std::move를 사용하기 위해 포함합니다.

#include <vector>
// updateColumns() 반환 타입으로 사용합니다.

namespace model
{

class HealthCheckLogModel final
{
  public:
    using PrimaryKeyType = std::uint64_t;
    // health_check_logs.id는 BIGINT AUTO_INCREMENT라 넉넉한 정수 타입을 사용합니다.

    inline static const std::string tableName = "health_check_logs";
    // 이 모델이 매핑되는 DB 테이블 이름입니다.

    inline static const std::string primaryKeyName = "id";
    // 로그 테이블의 기본키 컬럼입니다.

    explicit HealthCheckLogModel(std::string note)
        : id_(0), note_(std::move(note))
    {
        // 새 로그 row를 코드에서 만들 때 사용합니다. id는 DB가 INSERT 시 자동으로 채웁니다.
    }

    explicit HealthCheckLogModel(const drogon::orm::Row &row)
        : id_(row["id"].as<std::uint64_t>()),
          note_(row["note"].as<std::string>())
    {
        // 조회된 DB Row를 C++ 로그 객체로 변환합니다.
    }

    static std::string sqlForFindingByPrimaryKey()
    {
        // Mapper::findByPrimaryKey()에서 사용할 조회 SQL입니다.
        return "select `id`, `note` from `health_check_logs` where `id` = ?";
    }

    static std::string getColumnName(size_t index)
    {
        // Mapper가 인덱스로 컬럼명을 요구할 때 사용하는 매핑입니다.
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
        // 로그는 note만 넣고, id와 created_at은 DB 기본값에 맡깁니다.
        needSelection = false;
        return "insert into `health_check_logs` (`note`) values (?)";
    }

    void outputArgs(drogon::orm::internal::SqlBinder &binder) const
    {
        // INSERT의 ? 자리에 로그 메시지를 바인딩합니다.
        binder << note_;
    }

    std::vector<std::string> updateColumns() const
    {
        // 이 테스트에서는 로그 row를 수정하지 않습니다.
        return {};
    }

    void updateArgs(drogon::orm::internal::SqlBinder &) const
    {
        // updateColumns()가 비어 있으므로 바인딩할 값도 없습니다.
    }

    std::uint64_t getPrimaryKey() const noexcept
    {
        return id_;
    }

    void updateId(std::uint64_t id) noexcept
    {
        // Mapper::insert() 후 MySQL의 AUTO_INCREMENT id를 모델 객체에 반영합니다.
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
    // DB가 자동으로 발급하는 로그 id입니다.

    std::string note_;
    // 이번 테스트 호출에 대한 간단한 설명입니다.
};

}  // namespace model
