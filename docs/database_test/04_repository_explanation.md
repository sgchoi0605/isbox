# repository 상세 설명

대상 파일:

```text
drogon-db-test2/src/repositories/HealthCheckRepository.h
```

Repository는 DB 작업의 순서를 한 곳에 모아둔 계층이다. `main.cpp`가 SQL과 ORM 세부사항을 몰라도 되게 만드는 역할을 한다.

현재 파일에는 두 클래스가 있다.

| 클래스 | 역할 |
| --- | --- |
| `HealthCheckTestResult` | DB 작업 결과를 HTTP 응답 JSON으로 바꿀 수 있게 묶는 결과 객체 |
| `HealthCheckRepository` | 테이블 준비, 저장, 로그 insert, row 조회를 담당하는 DB 접근 객체 |

## include 구간

```cpp
#include <drogon/orm/DbClient.h>
```

Drogon이 관리하는 DB 연결 객체 타입을 사용하기 위해 필요하다. Repository 생성자는 `drogon::orm::DbClientPtr`를 받는다.

```cpp
#include <drogon/orm/Mapper.h>
```

`drogon::orm::Mapper<T>`를 사용하기 위해 필요하다. 이 Mapper가 모델 객체를 DB row처럼 insert, update, select한다.

```cpp
#include <json/value.h>
```

`HealthCheckTestResult::toJson()`에서 JSON 응답 본문을 만들기 위해 사용한다.

```cpp
#include <cstdint>
```

로그 id를 `Json::UInt64`로 변환할 때 사용한다.

```cpp
#include <utility>
```

생성자에서 `std::move()`로 모델 객체를 이동하기 위해 사용한다.

```cpp
#include "../models/HealthCheckLogModel.h"
#include "../models/HealthCheckModel.h"
```

Repository가 저장하고 조회할 DB row model들이다.

## HealthCheckTestResult

`HealthCheckTestResult`는 Repository 작업 결과를 담는 작은 결과 객체다.

```cpp
HealthCheckTestResult(model::HealthCheckModel health,
                      model::HealthCheckLogModel log)
    : health_(std::move(health)), log_(std::move(log))
```

저장 후 다시 조회한 `health_checks` row와, 새로 insert한 로그 row를 함께 보관한다.

```cpp
Json::Value toJson() const
```

HTTP 응답에 사용할 JSON을 만든다.

```cpp
body["ok"] = true;
body["message"] = "success";
body["value"] = health_.ok();
body["row_id"] = health_.id();
body["row_note"] = health_.note();
body["log_insert_id"] = static_cast<Json::UInt64>(log_.id());
body["log_note"] = log_.note();
```

각 필드의 의미는 다음과 같다.

| JSON 필드        | 값의 출처          | 의미       |
| ---             | ---               | ---        |
| `ok`            | 고정값             | 요청 성공 여부 |
| `message`       | 고정값             | 성공 메시지 |
| `value`         | `health_.ok()`    | health row의 ok 값 |
| `row_id`        | `health_.id()`    | 조회된 health row id |
| `row_note`      | `health_.note()`  | 조회된 health row note |
| `log_insert_id` | `log_.id()`       | 새로 insert된 로그 id |
| `log_note`      | `log_.note()`     | 새로 insert된 로그 메시지 |

Repository가 DB 모델을 그대로 밖으로 노출하지 않고, 응답에 필요한 모양으로 한 번 변환한다는 점이 중요하다.

## HealthCheckRepository 생성자

```cpp
explicit HealthCheckRepository(drogon::orm::DbClientPtr dbClient)
    : dbClient_(std::move(dbClient))
```

외부에서 DB client를 주입받아 저장한다.

이 구조의 장점은 다음과 같다.

- Repository가 `config.json`을 직접 읽지 않는다.
- 테스트할 때 다른 DB client를 넣을 수 있다.
- `main.cpp`는 DB 작업 세부 구현을 몰라도 된다.

## runSimpleTest()

```cpp
HealthCheckTestResult runSimpleTest()
```

이 예제의 핵심 흐름이다.

### 1. 테이블 준비

```cpp
ensureTables();
```

`health_checks`, `health_check_logs` 테이블이 없으면 생성한다.

학습 예제에서는 편하지만 실제 서비스에서는 요청 중 DDL을 실행하지 않는 편이 좋다. 실제 서비스에서는 migration 파일 또는 배포 스크립트로 분리해야 한다.

### 2. health row 객체 생성

```cpp
model::HealthCheckModel health(1, 1, "success");
```

`health_checks`에 넣을 C++ row 객체를 만든다.

값의 의미는 다음과 같다.

| 인자     | 값          | 의미 |
| ---     | ---         | --- |
| `id`    | `1`         | 테스트용 고정 primary key |
| `ok`    | `1`         | 성공 상태 |
| `note`  | `"success"` | 상태 메시지 |

### 3. health row 저장

```cpp
save(health);
```

`save()`는 먼저 update를 시도하고, update된 row가 없으면 insert한다.

첫 요청에서는 `id = 1` row가 없으므로 insert된다. 이후 요청부터는 같은 row를 update한다.

### 4. 로그 객체 생성

```cpp
model::HealthCheckLogModel log("/db-test 호출로 기록한 샘플 로그");
```

`health_check_logs`에 넣을 로그 row 객체를 만든다. id는 DB가 자동 생성한다.

### 5. 로그 insert

```cpp
insert(log);
```

로그는 호출될 때마다 새 row로 insert한다. `Mapper::insert(log)` 이후 DB가 발급한 id가 `log.updateId()`를 통해 객체 안에 반영된다.

### 6. health row 재조회

```cpp
return HealthCheckTestResult(findHealthCheckById(health.id()), log);
```

방금 저장한 health row를 DB에서 다시 조회한다. 그 결과와 로그 객체를 묶어 `HealthCheckTestResult`로 반환한다.

## ensureTables()

```cpp
void ensureTables()
```

필요한 테이블이 없으면 생성한다.

```cpp
dbClient_->execSqlSync("CREATE TABLE IF NOT EXISTS ...");
```

`execSqlSync()`는 SQL을 동기 방식으로 실행한다. 호출이 끝날 때까지 현재 요청 처리가 기다린다.

학습 예제에서는 흐름이 단순해서 괜찮지만, 실제 서비스에서 요청마다 동기 DDL을 실행하는 것은 피해야 한다.

실제 서비스에서는 다음 위치 중 하나로 옮기는 것이 좋다.

- `backend/database/migrations/*.sql`
- 별도 migration 도구
- 배포 전에 실행하는 DB 초기화 스크립트

## save()

```cpp
void save(model::HealthCheckModel &health)
```

`HealthCheckModel`을 upsert처럼 저장한다. 정확히는 SQL `UPSERT`가 아니라 `UPDATE 후 INSERT fallback` 방식이다.

```cpp
drogon::orm::Mapper<model::HealthCheckModel> mapper(dbClient_);
```

`HealthCheckModel` 전용 Drogon Mapper를 만든다.

```cpp
const auto updatedRows = mapper.update(health);
```

기본키가 같은 row를 update한다. `HealthCheckModel::getPrimaryKey()`로 id를 얻고, `updateColumns()`와 `updateArgs()`를 사용해 `ok`, `note`를 수정한다.

```cpp
if (updatedRows == 0)
{
    mapper.insert(health);
}
```

수정된 row가 없으면 아직 DB에 없는 row라고 보고 insert한다.

이 방식은 예제에는 충분하지만 실제 동시성 상황에서는 주의해야 한다. 여러 요청이 동시에 같은 id를 insert하려 하면 primary key 충돌이 날 수 있다. 실제 서비스에서는 DB의 `INSERT ... ON DUPLICATE KEY UPDATE` 같은 방식도 고려할 수 있다.

## insert()

```cpp
void insert(model::HealthCheckLogModel &log)
```

로그 row를 새로 insert한다.

```cpp
drogon::orm::Mapper<model::HealthCheckLogModel> mapper(dbClient_);
mapper.insert(log);
```

`HealthCheckLogModel::sqlForInserting()`와 `outputArgs()`가 사용된다. insert 후 AUTO_INCREMENT id가 `log.updateId()`를 통해 반영된다.

## findHealthCheckById()

```cpp
model::HealthCheckModel findHealthCheckById(int id)
```

기본키로 `health_checks` row를 조회한다.

```cpp
drogon::orm::Mapper<model::HealthCheckModel> mapper(dbClient_);
return mapper.findByPrimaryKey(id);
```

`HealthCheckModel::sqlForFindingByPrimaryKey()` SQL이 사용되고, 조회 결과는 `HealthCheckModel(const Row &row)` 생성자를 통해 C++ 객체로 바뀐다.

## dbClient_

```cpp
drogon::orm::DbClientPtr dbClient_;
```

`config.json`의 `"default"` DB 연결이다. Repository 내부의 모든 DB 작업은 이 client를 통해 실행된다.

## 실제 서비스 Repository로 바꿀 때의 기준

현재 Repository는 학습용이라 테이블 생성, 저장, 조회, 응답 결과 조립을 모두 한다. 실제 서비스에서는 다음처럼 책임을 더 나누는 것이 좋다.

| 현재 코드 | 실제 서비스 권장 위치 |
| --- | --- |
| `ensureTables()` | migration SQL 또는 DB 초기화 스크립트 |
| `save()` | Repository 또는 Mapper |
| `insert()` | Repository 또는 Mapper |
| `findHealthCheckById()` | Repository |
| `HealthCheckTestResult::toJson()` | DTO 또는 Controller 응답 변환 |
| 비즈니스 판단 | Service |

권장 흐름은 다음과 같다.

```text
Controller
  -> Service
    -> Repository
      -> Drogon Mapper / SQL
        -> DB
```

