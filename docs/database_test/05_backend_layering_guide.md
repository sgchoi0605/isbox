# 실제 backend 폴더 적용 가이드

현재 `backend` 폴더는 다음 구조를 가지고 있다.

```text
backend/
  common.hpp
  config/
  controllers/
    MemberController.cpp
  mappers/
    MemberMapper.cpp
  models/
    MemberDTO.cpp
  services/
    MemberService.cpp
```

`drogon-db-test2` 예제를 실제 백엔드에 적용할 때는 `main.cpp`에 모든 코드를 두지 말고 역할별로 나누는 것이 좋다.

## 권장 요청 흐름

실제 서비스의 기본 흐름은 다음 순서가 가장 읽기 쉽다.

```text
HTTP 요청
  -> Controller
    -> Service
      -> Repository 또는 Mapper
        -> Model
          -> DB
```

응답은 반대로 올라온다.

```text
DB row
  -> Model
    -> Repository 또는 Mapper
      -> Service
        -> DTO
          -> Controller
            -> JSON 응답
```

## 각 폴더의 책임

### controllers

HTTP 계층이다.

담당할 일:

- URL, HTTP method 연결
- path parameter, query parameter, JSON body 읽기
- 기본적인 요청 형식 검증
- service 호출
- service 결과를 HTTP status와 JSON으로 변환

하지 말아야 할 일:

- SQL 작성
- DB client 직접 사용
- 복잡한 비즈니스 규칙 처리
- DB row model을 그대로 응답으로 노출

예시:

```text
controllers/MemberController.cpp
  POST /members/login
  GET  /members/{id}
  PATCH /members/{id}/profile
```

### services

비즈니스 흐름 계층이다.

담당할 일:

- 회원 가입, 로그인, 프로필 수정 같은 use case 처리
- 여러 Repository 호출을 하나의 작업으로 묶기
- 권한, 상태, 레벨, 경험치 같은 비즈니스 규칙 판단
- 트랜잭션이 필요하면 트랜잭션 경계 관리

하지 말아야 할 일:

- HTTP request/response 타입에 의존
- Drogon controller 세부 API에 의존
- 단순 SQL 문자열을 여기저기 직접 작성

예시:

```text
services/MemberService.cpp
  login(email, password)
  getProfile(memberId)
  addExp(memberId, amount)
```

### repositories

현재 `backend`에는 없지만, 실제 DB 접근을 명확히 나누려면 추가하는 것을 권장한다.

담당할 일:

- 특정 aggregate 또는 table 중심의 DB 조회/저장
- Drogon `DbClientPtr` 보관
- Drogon `Mapper<T>` 사용
- 복잡한 SELECT/INSERT/UPDATE SQL 캡슐화

예시:

```text
repositories/MemberRepository.h
repositories/MemberRepository.cpp
repositories/HealthCheckRepository.h
repositories/HealthCheckRepository.cpp
```

`drogon-db-test2/src/repositories/HealthCheckRepository.h`는 실제 백엔드에서는 이 폴더로 옮기는 것이 가장 자연스럽다.

### mappers

`mappers`는 프로젝트에서 의미를 명확히 정해야 한다. 두 가지 선택지가 있다.

첫 번째 선택지는 `mappers`를 DB 접근 계층으로 쓰는 방식이다. 이 경우 `repositories` 폴더를 따로 만들지 않고 다음 흐름을 쓴다.

```text
Controller -> Service -> Mapper -> DB
```

두 번째 선택지는 `repositories`를 DB 접근 계층으로 두고, `mappers`는 변환 전용으로 쓰는 방식이다.

```text
Controller -> Service -> Repository -> DB
                         Mapper -> DTO 변환
```

이 프로젝트는 `drogon-db-test2`에서 이미 Repository 패턴을 쓰고 있으므로, 장기적으로는 두 번째 방식을 권장한다.

이때 `mappers`는 다음 역할에 집중한다.

- DB model을 DTO로 변환
- DTO를 domain model로 변환
- 여러 model을 응답 DTO 하나로 조립

예시:

```text
mappers/MemberMapper.cpp
  MemberEntity -> MemberDTO
  MemberEntity + LevelEntity -> MemberProfileDTO
```

### models

데이터 구조 계층이다.

현재 `backend/models/MemberDTO.cpp`는 회원 응답/전달용 DTO처럼 쓰이고 있다. 실제 서비스가 커지면 모델을 더 나누는 것이 좋다.

권장 분리:

```text
models/
  entities/
    Member.h
    HealthCheck.h
  dto/
    MemberDTO.h
    LoginRequestDTO.h
    LoginResponseDTO.h
  orm/
    HealthCheckModel.h
    HealthCheckLogModel.h
```

각 모델의 의미는 다음과 같다.

| 종류 | 역할 |
| --- | --- |
| Entity | 서비스 내부에서 쓰는 핵심 데이터 구조 |
| DTO | controller 입출력용 데이터 구조 |
| ORM Model | Drogon Mapper가 DB row와 직접 매핑하는 구조 |

처음에는 너무 많이 쪼개지 않아도 된다. 하지만 DB row와 HTTP 응답 모양이 달라지는 순간 DTO와 ORM model은 분리하는 것이 좋다.

## drogon-db-test2 코드를 backend로 옮긴다면

학습 예제를 실제 폴더로 옮긴다면 다음처럼 나눌 수 있다.

```text
backend/
  app/
    main.cpp
  config/
    config.json
  controllers/
    HealthCheckController.h
    HealthCheckController.cpp
  services/
    HealthCheckService.h
    HealthCheckService.cpp
  repositories/
    HealthCheckRepository.h
    HealthCheckRepository.cpp
  models/
    orm/
      HealthCheckModel.h
      HealthCheckLogModel.h
    dto/
      HealthCheckResponseDTO.h
  database/
    migrations/
      001_create_health_check_tables.sql
    MariaDbRuntime.h
    MariaDbRuntime.cpp
```

## 파일별 이동 기준

| 현재 위치 | 실제 backend 위치 | 이유 |
| --- | --- | --- |
| `src/main.cpp`의 서버 실행 코드 | `backend/app/main.cpp` | 앱 시작점만 남긴다. |
| `src/main.cpp`의 `/db-test` lambda | `backend/controllers/HealthCheckController.cpp` | HTTP route는 controller 책임이다. |
| `makeJsonResponse()` | 공통 response helper 또는 controller 내부 | 여러 controller에서 쓰면 공통 helper로 뺀다. |
| `makeErrorBody()` | 공통 error response helper | 오류 응답 형식을 통일한다. |
| `configureMariaDbRuntimeForWindows()` | `backend/database/MariaDbRuntime.cpp` | DB 런타임 준비는 database infrastructure 책임이다. |
| `HealthCheckRepository` | `backend/repositories/HealthCheckRepository.*` | DB 접근은 repository 책임이다. |
| `HealthCheckModel` | `backend/models/orm/HealthCheckModel.h` | Drogon ORM용 row model이다. |
| `HealthCheckLogModel` | `backend/models/orm/HealthCheckLogModel.h` | Drogon ORM용 row model이다. |
| `HealthCheckTestResult::toJson()` | `backend/models/dto/HealthCheckResponseDTO.h` 또는 Mapper | 응답 모양은 DTO로 분리하는 편이 좋다. |
| `ensureTables()` | `backend/database/migrations/*.sql` | 테이블 생성은 요청 처리와 분리한다. |

## Member 기능에 적용한 예시 구조

회원 기능을 실제로 만든다면 다음처럼 시작할 수 있다.

```text
backend/
  controllers/
    MemberController.h
    MemberController.cpp
  services/
    MemberService.h
    MemberService.cpp
  repositories/
    MemberRepository.h
    MemberRepository.cpp
  mappers/
    MemberMapper.h
    MemberMapper.cpp
  models/
    entities/
      Member.h
    dto/
      MemberDTO.h
      LoginRequestDTO.h
      LoginResponseDTO.h
    orm/
      MemberModel.h
  database/
    migrations/
      001_create_members.sql
```

역할은 다음과 같이 나눈다.

| 파일 | 책임 |
| --- | --- |
| `MemberController` | `/members`, `/login` 같은 HTTP endpoint 처리 |
| `MemberService` | 로그인 성공/실패 판단, 회원 상태 확인, 경험치/레벨 규칙 처리 |
| `MemberRepository` | member table 조회, insert, update |
| `MemberMapper` | `MemberModel`을 `MemberDTO`로 변환 |
| `MemberModel` | Drogon ORM Mapper가 사용하는 DB row model |
| `MemberDTO` | frontend로 내려줄 회원 데이터 |
| migration SQL | member table 생성/변경 |

## main.cpp는 어디까지 가져야 하나

실제 서비스의 `main.cpp`는 다음 정도만 가지는 것이 좋다.

```cpp
int main()
{
    configureMariaDbRuntimeForWindowsIfNeeded();
    drogon::app().loadConfigFile("config.json");
    drogon::app().run();
}
```

라우터가 많아질수록 `main.cpp`에 `registerHandler()` lambda를 계속 추가하면 파일이 빠르게 복잡해진다. Controller 클래스로 옮기는 편이 유지보수에 좋다.

## Repository와 Mapper를 둘 다 둘지 결정하는 기준

처음에는 다음 기준으로 결정하면 된다.

| 상황 | 추천 |
| --- | --- |
| CRUD가 단순하고 테이블 수가 적다 | `Service -> Mapper`만 사용해도 충분하다. |
| 쿼리가 복잡해진다 | `Service -> Repository -> Mapper`로 나눈다. |
| 여러 테이블을 조합해서 조회한다 | Repository를 둔다. |
| DB model과 응답 DTO 변환이 많다 | Mapper를 변환 전용으로 둔다. |
| 테스트 코드에서 DB 접근을 가짜로 바꾸고 싶다 | Repository interface를 두는 것이 유리하다. |

이 프로젝트에서는 앞으로 회원, 레시피, 재료, 커뮤니티처럼 기능이 늘어날 가능성이 있으므로 다음 구조를 추천한다.

```text
Controller -> Service -> Repository -> Drogon ORM Model
                         Mapper -> DTO
```

## 꼭 지켜야 할 경계

- Controller는 DB를 직접 만지지 않는다.
- Service는 HTTP response 객체를 만들지 않는다.
- Repository는 비즈니스 규칙을 판단하지 않는다.
- Model은 가능한 데이터 구조와 DB 매핑 규칙만 가진다.
- DTO는 frontend와 주고받을 모양에 맞춘다.
- 테이블 생성/변경은 코드 안에서 매 요청마다 하지 말고 migration으로 관리한다.

이 경계를 지키면 기능이 늘어나도 파일이 어디에 있어야 하는지 판단하기 쉬워진다.

