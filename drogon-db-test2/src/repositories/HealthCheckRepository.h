#pragma once
// health check 테스트에서 필요한 DB 작업을 한 곳에 모아 둔 Repository입니다.

#include <drogon/orm/DbClient.h>
// Drogon이 관리하는 DB 연결 객체 타입입니다.

#include <drogon/orm/Mapper.h>
// C++ 모델 객체를 DB row처럼 저장/조회하는 ORM Mapper입니다.

#include <json/value.h>
// 테스트 결과를 HTTP 응답 JSON으로 바꾸기 위해 사용합니다.

#include <cstdint>
// 로그 id를 JSON 정수로 변환할 때 사용합니다.

#include <utility>
// 생성자에서 DB 클라이언트 포인터를 이동하기 위해 사용합니다.

#include "../models/HealthCheckLogModel.h"
#include "../models/HealthCheckModel.h"
// Repository가 다룰 DB row 모델들입니다.

namespace repository
{

class HealthCheckTestResult final
{
  private:
    model::HealthCheckModel health_;
    // health_checks 테이블에서 ORM으로 다시 읽어 온 대표 상태 row입니다.

    model::HealthCheckLogModel log_;
    // health_check_logs 테이블에 INSERT한 이번 호출 로그 row입니다.


  public:
    HealthCheckTestResult(model::HealthCheckModel health,
                          model::HealthCheckLogModel log)
        : health_(std::move(health)), log_(std::move(log))
    {
        // 테스트 흐름에서 저장/조회한 row 객체들을 응답용 결과 객체로 묶습니다.
    }

    Json::Value toJson() const
    {
        // HTTP 응답으로 보낼 JSON을 구성합니다.
        // main.cpp가 DB 모델 내부 구조를 몰라도 되도록 변환 책임을 여기에 둡니다.
        Json::Value body;
        body["ok"] = true;
        body["message"] = "success";
        body["value"] = health_.ok();
        body["row_id"] = health_.id();
        body["row_note"] = health_.note();
        body["log_insert_id"] = static_cast<Json::UInt64>(log_.id());
        body["log_note"] = log_.note();
        return body;
    }


};

class HealthCheckRepository final
{
  public:
    explicit HealthCheckRepository(drogon::orm::DbClientPtr dbClient)
        : dbClient_(std::move(dbClient))
    {
        // Repository는 DB 클라이언트를 받아 Mapper들을 만들어 사용합니다.
    }

    HealthCheckTestResult runSimpleTest()
    {
        // 가장 단순한 ORM 테스트 흐름입니다.
        // 1. 테이블을 준비한다.
        // 2. C++ 객체를 만든다.
        // 3. 객체를 DB row로 저장한다.
        // 4. 저장된 row를 다시 C++ 객체로 조회한다.
        ensureTables();

        model::HealthCheckModel health(1, 1, "success");
        save(health);
        // SQL 문자열 대신 C++ 객체를 만들어 DB row처럼 저장합니다.

        model::HealthCheckLogModel log("/db-test 호출로 기록된 샘플 로그");
        insert(log);
        // AUTO_INCREMENT id는 Mapper::insert() 후 log 객체 안에 반영됩니다.

        return HealthCheckTestResult(findHealthCheckById(health.id()), log);
    }

  private:
    void ensureTables()
    {
        // ORM은 row 조작을 읽기 좋게 만들어 주지만, 테이블 생성 같은 DDL은 SQL이 필요합니다.
        // 그래서 DDL은 Repository 안에 숨기고 main.cpp 밖으로 빼서 라우터를 단순하게 유지합니다.
        dbClient_->execSqlSync(
            "CREATE TABLE IF NOT EXISTS `health_checks` ("
            "`id` INT NOT NULL,"
            "`ok` INT NOT NULL,"
            "`note` VARCHAR(255) NOT NULL,"
            "PRIMARY KEY (`id`)"
            ")");

        dbClient_->execSqlSync(
            "CREATE TABLE IF NOT EXISTS `health_check_logs` ("
            "`id` BIGINT NOT NULL AUTO_INCREMENT,"
            "`note` VARCHAR(255) NOT NULL,"
            "`created_at` DATETIME NOT NULL DEFAULT CURRENT_TIMESTAMP,"
            "PRIMARY KEY (`id`)"
            ")");
    }

    void save(model::HealthCheckModel &health)
    {
        // 전통적인 ORM 느낌의 save입니다.
        // 먼저 UPDATE를 시도하고, 바뀐 row가 없으면 INSERT합니다.
        drogon::orm::Mapper<model::HealthCheckModel> mapper(dbClient_);

        const auto updatedRows = mapper.update(health);
        if (updatedRows == 0)
        {
            mapper.insert(health);
        }
    }

    void insert(model::HealthCheckLogModel &log)
    {
        // 로그 row는 매 호출마다 새로 쌓이므로 INSERT만 합니다.
        drogon::orm::Mapper<model::HealthCheckLogModel> mapper(dbClient_);
        mapper.insert(log);
    }

    model::HealthCheckModel findHealthCheckById(int id)
    {
        // 기본키로 한 row를 조회하면 Mapper가 HealthCheckModel 객체로 바꿔 줍니다.
        drogon::orm::Mapper<model::HealthCheckModel> mapper(dbClient_);
        return mapper.findByPrimaryKey(id);
    }

    drogon::orm::DbClientPtr dbClient_;
    // config.json의 "default" DB 연결입니다.
};

}  // namespace repository
