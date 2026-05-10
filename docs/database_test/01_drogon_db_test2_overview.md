# drogon-db-test2 전체 흐름

## 프로젝트의 목적

`drogon-db-test2`는 Drogon C++ 서버에서 MySQL/MariaDB에 접속하고, Drogon ORM `Mapper`로 C++ 객체를 DB row처럼 저장하고 다시 조회하는 예제다.

현재 구조는 다음과 같다.

```text
drogon-db-test2/
  CMakeLists.txt
  config.json
  src/
    main.cpp
    models/
      HealthCheckModel.h
      HealthCheckLogModel.h
    repositories/
      HealthCheckRepository.h
```

각 영역의 역할은 다음과 같다.

| 영역 | 역할 |
| --- | --- |
| `config.json` | HTTP listener와 DB 접속 정보를 정의한다. 코드에서는 `getDbClient("default")`로 DB 연결을 가져온다. |
| `main.cpp` | 서버 시작, 설정 로드, `/db-test` 라우터 등록, JSON 응답 생성, 예외 처리를 담당한다. |
| `models` | DB 테이블 row를 C++ 객체로 표현한다. Drogon ORM `Mapper`가 호출할 함수들을 제공한다. |
| `repositories` | DB 작업의 세부 흐름을 한 곳에 모은다. 테이블 생성, 저장, 조회, JSON 응답용 결과 조립을 담당한다. |

## 요청이 들어왔을 때의 전체 흐름

브라우저 또는 클라이언트가 다음 주소로 GET 요청을 보낸다고 가정한다.

```text
GET http://127.0.0.1:8080/db-test
```

처리 순서는 다음과 같다.

1. Drogon이 `/db-test`에 등록된 handler를 찾는다.
2. `main.cpp`의 lambda handler가 실행된다.
3. handler 안에서 `drogon::app().getDbClient("default")`로 DB 연결을 가져온다.
4. 그 DB 연결을 `HealthCheckRepository` 생성자에 넘긴다.
5. `repository.runSimpleTest()`를 호출한다.
6. Repository가 `health_checks`, `health_check_logs` 테이블을 준비한다.
7. `HealthCheckModel` 객체를 만들고 `health_checks`에 저장한다.
8. `HealthCheckLogModel` 객체를 만들고 `health_check_logs`에 새 로그를 저장한다.
9. 저장된 `health_checks` row를 다시 조회한다.
10. 조회 결과와 로그 결과를 `HealthCheckTestResult`에 담는다.
11. `HealthCheckTestResult::toJson()`이 HTTP 응답 JSON을 만든다.
12. `main.cpp`가 `makeJsonResponse()`로 Drogon 응답 객체를 만들고 callback으로 반환한다.

## 코드 레벨 호출 흐름

```text
main()
  configureMariaDbRuntimeForWindows()
  drogon::app().loadConfigFile("config.json")
  drogon::app().registerHandler("/db-test", handler, {Get})
  drogon::app().run()

/db-test handler
  HealthCheckRepository repository(app().getDbClient("default"))
  HealthCheckTestResult result = repository.runSimpleTest()
  callback(makeJsonResponse(result.toJson()))

HealthCheckRepository::runSimpleTest()
  ensureTables()
  HealthCheckModel health(1, 1, "success")
  save(health)
  HealthCheckLogModel log("...")
  insert(log)
  return HealthCheckTestResult(findHealthCheckById(health.id()), log)
```

## DB 테이블 흐름

예제에서 사용하는 테이블은 2개다.

### health_checks

```sql
CREATE TABLE IF NOT EXISTS `health_checks` (
  `id` INT NOT NULL,
  `ok` INT NOT NULL,
  `note` VARCHAR(255) NOT NULL,
  PRIMARY KEY (`id`)
)
```

이 테이블은 테스트용 대표 row를 저장한다. 예제에서는 항상 `id = 1`인 row를 사용한다.

첫 요청에서는 row가 없기 때문에 `UPDATE`가 0건 처리되고, 그 뒤 `INSERT`가 실행된다. 두 번째 요청부터는 이미 `id = 1` row가 있으므로 `UPDATE`만 성공한다.

### health_check_logs

```sql
CREATE TABLE IF NOT EXISTS `health_check_logs` (
  `id` BIGINT NOT NULL AUTO_INCREMENT,
  `note` VARCHAR(255) NOT NULL,
  `created_at` DATETIME NOT NULL DEFAULT CURRENT_TIMESTAMP,
  PRIMARY KEY (`id`)
)
```

이 테이블은 `/db-test`가 호출될 때마다 새 로그 row를 남긴다. `id`는 DB가 `AUTO_INCREMENT`로 발급하고, `created_at`은 DB 기본값으로 들어간다.

## 응답 JSON

정상 응답은 대략 다음 형태다.

```json
{
  "ok": true,
  "message": "success",
  "value": 1,
  "row_id": 1,
  "row_note": "success",
  "log_insert_id": 1,
  "log_note": "/db-test 호출로 기록한 샘플 로그"
}
```

오류가 발생하면 `main.cpp`에서 다음 형태로 내려준다.

```json
{
  "ok": false,
  "message": "MySQL ORM 테스트 실패",
  "error": "실제 DB 예외 메시지"
}
```

## 이 예제에서 중요한 설계 포인트

- `main.cpp`는 HTTP 서버와 라우팅만 담당하고, SQL 세부 구현은 Repository에 맡긴다.
- `models`는 DB row 구조와 Drogon ORM Mapper가 요구하는 함수들을 제공한다.
- `repositories`는 실제 DB 작업 순서를 담당한다.
- `HealthCheckTestResult`는 DB 모델을 바로 응답으로 노출하지 않고, 응답 JSON 형태로 변환하는 중간 결과 객체 역할을 한다.
- `ensureTables()`는 학습 예제에서는 편하지만, 실제 서비스에서는 요청마다 실행하기보다 migration 파일로 분리하는 것이 좋다.

