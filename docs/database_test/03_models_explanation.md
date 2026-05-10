# models 상세 설명

대상 파일:

```text
drogon-db-test2/src/models/HealthCheckModel.h
drogon-db-test2/src/models/HealthCheckLogModel.h
```

이 폴더의 모델들은 일반 DTO만이 아니라 Drogon ORM `Mapper<T>`가 사용할 수 있는 row model이다.

Drogon `Mapper<model::HealthCheckModel>`이 insert, update, findByPrimaryKey 같은 작업을 하려면 모델 클래스가 정해진 함수와 타입을 제공해야 한다. 현재 모델들이 그 계약을 직접 구현하고 있다.

## 공통 include의 의미

```cpp
#include <drogon/orm/Row.h>
```

DB 조회 결과 한 줄을 C++ 객체로 바꾸기 위해 필요하다. `HealthCheckModel(const drogon::orm::Row &row)` 같은 생성자에서 사용한다.

```cpp
#include <drogon/orm/SqlBinder.h>
```

`INSERT`, `UPDATE` SQL의 `?` 자리에 C++ 값을 바인딩할 때 사용한다.

```cpp
#include <cstdint>
```

`std::uint64_t` 같은 고정 크기 정수 타입을 사용하기 위해 포함한다. 특히 `HealthCheckLogModel`의 `BIGINT AUTO_INCREMENT` id에 사용된다.

```cpp
#include <stdexcept>
```

잘못된 컬럼 index가 들어왔을 때 `std::out_of_range`를 던지기 위해 사용한다.

```cpp
#include <string>
```

`note` 같은 문자열 필드를 표현하기 위해 사용한다.

```cpp
#include <utility>
```

생성자에서 `std::move()`로 문자열 소유권을 이동하기 위해 사용한다.

```cpp
#include <vector>
```

`updateColumns()`가 수정 대상 컬럼 목록을 `std::vector<std::string>`으로 반환하기 때문에 필요하다.

## HealthCheckModel 역할

`HealthCheckModel`은 `health_checks` 테이블의 row 하나를 표현한다.

DB 테이블 기준 구조는 다음과 같다.

| DB 컬럼 | C++ 필드 | 의미                                 |
| ---    | ---      | ---                                 |
| `id`   | `id_`    | 기본키. 예제에서는 항상 `1`을 사용한다.  |
| `ok`   | `ok_`    | 테스트 성공 여부를 숫자로 저장한다.      |
| `note` | `note_`  | 상태 메시지다.                        |

## HealthCheckModel의 Mapper 계약

```cpp
using PrimaryKeyType = int;
```

기본키 타입이 `int`임을 Drogon Mapper에 알려준다.

```cpp
inline static const std::string tableName = "health_checks";
```

이 모델이 매핑되는 DB 테이블 이름이다.

```cpp
inline static const std::string primaryKeyName = "id";
```

기본키 컬럼 이름이다.

```cpp
HealthCheckModel(int id, int ok, std::string note)
```

코드에서 새 row 객체를 직접 만들 때 쓰는 생성자다.

```cpp
explicit HealthCheckModel(const drogon::orm::Row &row)
```

DB 조회 결과를 C++ 객체로 바꾸는 생성자다.
HealthCheckModel 객체를 만들 때, drogon::orm::Row 하나를 받아서 만들 수 있다.
explicit은 자동 형변환을 막는 키워드

drogon::orm::Row	Drogon ORM에서 DB 조회 결과 한 줄을 나타내는 타입


```cpp
id_(row["id"].as<int>())
ok_(row["ok"].as<int>())
note_(row["note"].as<std::string>())
```
객체가 만들어질 때 멤버 변수들을 바로 초기화하는 부분

DB row의 각 컬럼을 읽어 C++ 필드에 넣는다. 컬럼 이름이 SQL 조회 결과와 맞지 않으면 여기서 문제가 난다.

.as<타입> 은 타입으로 바꿔라. 라는 뜻


```cpp
static std::string sqlForFindingByPrimaryKey()
```

`Mapper::findByPrimaryKey()`가 사용할 SQL을 반환한다.

```sql
select `id`, `ok`, `note` from `health_checks` where `id` = ?
```

`?` 자리에는 `findByPrimaryKey(id)`로 넘긴 기본키 값이 바인딩된다.

```cpp
static std::string getColumnName(size_t index)
```

Drogon Mapper가 컬럼 index를 컬럼 이름으로 바꿔야 할 때 사용한다.

현재 mapping은 다음과 같다.

| index | column |
| --- | --- |
| 0 | `id` |
| 1 | `ok` |
| 2 | `note` |

범위를 벗어난 index가 들어오면 `std::out_of_range`를 던진다. 잘못된 Mapper 사용을 조용히 넘기지 않기 위한 방어 코드다.

```cpp
std::string sqlForInserting(bool &needSelection) const
```

`Mapper::insert()`가 사용할 INSERT SQL을 반환한다.

```cpp
needSelection = false;
return "insert into `health_checks` (`id`, `ok`, `note`) values (?, ?, ?)";
```

`needSelection = false`는 insert 후 별도 select가 필요 없다는 뜻이다. 이 모델은 id를 DB가 자동 생성하지 않고 코드에서 직접 넣기 때문에 별도 조회가 필요 없다.

```cpp
void outputArgs(drogon::orm::internal::SqlBinder &binder) const
```

INSERT SQL의 `?` 자리에 들어갈 값을 순서대로 바인딩한다.

```cpp
binder << id_ << ok_ << note_;
```

이 순서는 `values (?, ?, ?)`와 반드시 맞아야 한다.

```cpp
std::vector<std::string> updateColumns() const
```

`Mapper::update()`가 수정할 컬럼 목록을 반환한다.

```cpp
return {"ok", "note"};
```

`id`는 기본키이므로 수정 대상에서 제외한다.

```cpp
void updateArgs(drogon::orm::internal::SqlBinder &binder) const
```

UPDATE의 SET 절에 들어갈 값을 바인딩한다.

```cpp
binder << ok_ << note_;
```

이 순서는 `updateColumns()`의 순서와 맞아야 한다.

```cpp
int getPrimaryKey() const noexcept
```

`Mapper::update()`가 `WHERE id = ?` 조건을 만들 때 사용할 기본키 값을 반환한다.

```cpp
void updateId(std::uint64_t)
```

`Mapper::insert()` 이후 insert id를 모델에 반영할 때 호출될 수 있는 함수다. `HealthCheckModel`은 id를 코드에서 직접 지정하므로 아무 일도 하지 않는다.

```cpp
int id() const noexcept
int ok() const noexcept
const std::string &note() const noexcept
```

외부에서 private 필드를 읽기 위한 getter다.



## HealthCheckLogModel 역할

`HealthCheckLogModel`은 `health_check_logs` 테이블의 row 하나를 표현한다.

DB 테이블 기준 구조는 다음과 같다.

| DB 컬럼 | C++ 필드 | 의미 |
| --- | --- | --- |
| `id` | `id_` | `BIGINT AUTO_INCREMENT` 기본키 |
| `note` | `note_` | 로그 메시지 |
| `created_at` | C++ 필드 없음 | DB 기본값으로 생성되는 시각 |

`created_at`은 테이블에는 있지만 모델 필드에는 없다. 현재 예제 응답에서 `created_at`을 사용하지 않기 때문이다.

## HealthCheckLogModel의 Mapper 계약

```cpp
using PrimaryKeyType = std::uint64_t;
```

로그 id는 `BIGINT`이므로 C++에서는 `std::uint64_t`로 표현한다.

```cpp
inline static const std::string tableName = "health_check_logs";
inline static const std::string primaryKeyName = "id";
```

테이블 이름과 기본키 컬럼 이름이다.

```cpp
explicit HealthCheckLogModel(std::string note)
    : id_(0), note_(std::move(note))
```

새 로그를 만들 때 쓰는 생성자다. `id_`는 아직 DB가 발급하지 않았으므로 `0`으로 둔다.

```cpp
explicit HealthCheckLogModel(const drogon::orm::Row &row)
```

DB 조회 결과 row를 C++ 로그 객체로 바꾸는 생성자다.

```cpp
static std::string sqlForFindingByPrimaryKey()
```

기본키로 로그를 조회할 때 사용할 SQL을 반환한다.

```sql
select `id`, `note` from `health_check_logs` where `id` = ?
```

```cpp
static std::string getColumnName(size_t index)
```

컬럼 index와 컬럼 이름의 대응 관계를 제공한다.

| index | column |
| --- | --- |
| 0 | `id` |
| 1 | `note` |

```cpp
std::string sqlForInserting(bool &needSelection) const
```

로그 insert SQL을 반환한다.

```cpp
needSelection = false;
return "insert into `health_check_logs` (`note`) values (?)";
```

`id`와 `created_at`은 DB가 자동으로 채우므로 `note`만 insert한다.

```cpp
void outputArgs(drogon::orm::internal::SqlBinder &binder) const
```

로그 메시지만 바인딩한다.

```cpp
binder << note_;
```

```cpp
std::vector<std::string> updateColumns() const
```

현재 예제에서는 로그 row를 수정하지 않기 때문에 빈 목록을 반환한다.

```cpp
void updateArgs(drogon::orm::internal::SqlBinder &) const
```

수정할 컬럼이 없으므로 아무 값도 바인딩하지 않는다.

```cpp
void updateId(std::uint64_t id) noexcept
```

`Mapper::insert(log)` 이후 DB가 발급한 AUTO_INCREMENT id를 `log.id_`에 반영한다.

이 함수가 있기 때문에 Repository에서 insert 후 바로 `log.id()`를 응답에 넣을 수 있다.

## 두 모델의 차이

| 항목 | HealthCheckModel | HealthCheckLogModel |
| --- | --- | --- |
| 테이블 | `health_checks` | `health_check_logs` |
| 기본키 타입 | `int` | `std::uint64_t` |
| id 생성 방식 | 코드에서 직접 지정 | DB AUTO_INCREMENT |
| insert 컬럼 | `id`, `ok`, `note` | `note` |
| update 대상 | `ok`, `note` | 없음 |
| 주 용도 | 현재 상태 row 저장/조회 | 호출 이력 로그 저장 |

## 실제 서비스 모델로 확장할 때 주의할 점

- DB 컬럼 이름과 `Row`에서 읽는 이름이 반드시 같아야 한다.
- `outputArgs()`의 바인딩 순서는 INSERT SQL의 `?` 순서와 같아야 한다.
- `updateArgs()`의 바인딩 순서는 `updateColumns()` 순서와 같아야 한다.
- AUTO_INCREMENT id를 쓰는 모델은 `updateId()`에서 insert id를 반영해야 한다.
- 응답 전용 DTO와 DB row model은 목적이 다르므로, 서비스가 커지면 분리하는 것이 좋다.

