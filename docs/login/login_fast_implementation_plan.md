# 로그인 빠른 구현 설계

## 목적

이 문서는 현재 `frontend`의 HTML, CSS, JS 구조를 기준으로 사용자 `Member` 로그인을 빠르게 실제 DB 기반 기능으로 바꾸기 위한 설계 문서다.

참조 기준:

- `frontend/pages/index.html`
- `frontend/pages/profile.html`
- `frontend/js/app.js`
- `frontend/js/modules/auth.js`
- `frontend/js/modules/profile.js`
- `frontend/js/modules/state.js`
- `frontend/js/modules/storage.js`
- `frontend/js/modules/ui.js`
- `frontend/css/style.css`
- `docs/database_test/*`
- `docs/DB_schema/login_schema.md`
- `backend/models/MemberDTO.cpp`

이번 1차 목표는 회원가입, 비밀번호 찾기, 프로필 수정까지 완성하는 것이 아니라 **로그인만 빠르게 실제 DB와 연결하는 것**이다. 단, 현재 프론트 화면에 회원가입 탭과 프로필 화면이 이미 있으므로 이후 확장 방향도 함께 정리한다.

## 가장 중요한 결론

현재 프론트엔드는 이미 로그인 화면, 사용자 메뉴, 프로필 화면, 토스트 알림, 로그인 필요 페이지 차단 구조가 있다. 따라서 UI를 새로 만들 필요는 거의 없다.

빠른 구현은 다음 순서로 가는 것이 좋다.

```text
1. DB에 members 테이블 준비
2. backend에 MemberModel, MemberRepository, MemberService, MemberController 추가
3. POST /api/auth/login 구현
4. frontend/js/modules/auth.js의 mock login()을 fetch 기반 로그인으로 교체
5. 로그인 성공 시 기존 localStorage.user 구조와 호환되는 객체 저장
6. 기존 requireAuth(), displayUserInfo(), user menu는 그대로 사용
```

## 객체 이름 규칙

프로젝트 클래스 객체를 만들 때 변수명은 **클래스 이름과 같게 하되 첫 글자만 소문자**로 한다.

예시:

```cpp
MemberController memberController;
MemberService memberService;
MemberRepository memberRepository;
MemberMapper memberMapper;
MemberModel memberModel;
MemberDTO memberDTO;
LoginRequestDTO loginRequestDTO;
LoginResponseDTO loginResponseDTO;
MemberSessionModel memberSessionModel;
MemberLoginHistoryModel memberLoginHistoryModel;
```

이 규칙 때문에 다음처럼 쓰지 않는다.

```cpp
MemberRepository repository;      // 사용하지 않음
MemberService service;            // 사용하지 않음
LoginRequestDTO requestDTO;       // 사용하지 않음
MemberDTO user;                   // 사용하지 않음
```

`database_test` 문서의 예제에는 `HealthCheckRepository repository(...)`처럼 설명용 변수명이 나오지만, 실제 `Member` 기능을 구현할 때는 이 프로젝트 규칙을 우선 적용한다.

```cpp
repository::MemberRepository memberRepository(
    drogon::app().getDbClient("default"));

service::MemberService memberService(memberRepository);

dto::LoginRequestDTO loginRequestDTO(requestJson);
dto::LoginResponseDTO loginResponseDTO =
    memberService.login(loginRequestDTO);
```

표준 타입이나 원시 타입은 이 규칙을 강제하지 않고 의미가 분명한 이름을 쓴다.

```cpp
std::string email;
std::string password;
bool rememberMe;
```

## 현재 프론트엔드 분석

### 1. 로그인 화면

파일:

```text
frontend/pages/index.html
```

현재 로그인 화면에는 두 탭이 있다.

- 로그인 탭
- 회원가입 탭

로그인 form은 다음 입력값을 사용한다.

| HTML id | 의미 |
| --- | --- |
| `loginEmail` | 로그인 이메일 |
| `loginPassword` | 로그인 비밀번호 |
| 이름 없는 checkbox | 로그인 유지 |

현재 submit은 다음 JS 함수를 호출한다.

```html
app.login(
  document.getElementById('loginEmail').value,
  document.getElementById('loginPassword').value
)
```

현재 `login()` 함수는 서버를 호출하지 않고, 이메일 앞부분으로 이름을 만들어 `localStorage.user`에 저장한다.

```js
const user = {
  email: email,
  name: email.split('@')[0],
  loggedIn: true
};
```

빠른 구현에서는 이 form은 유지하고 `auth.js`의 `login()` 내부만 API 호출로 바꾸면 된다.

다만 `로그인 유지` 체크박스는 현재 `id`가 없다. 실제 API 요청에 포함하려면 다음처럼 id를 추가하는 편이 좋다.

```html
<input type="checkbox" id="rememberMe">
```

그 후 `login()` 내부에서 다음처럼 읽는다.

```js
const rememberMe = document.getElementById('rememberMe')?.checked ?? false;
```

### 2. 로그인 이후 페이지 접근 제어

파일:

```text
frontend/js/app.js
```

현재 `DOMContentLoaded`에서 로그인 페이지가 아닌 경우 다음 흐름을 실행한다.

```js
if (!window.location.pathname.includes('index.html') &&
    window.location.pathname !== '/' &&
    !window.location.pathname.endsWith('/')) {
  if (!requireAuth()) return;
  displayUserInfo();
  checkExpiringItems();
  activateCurrentNav();
  loadLevelData();
  updateLevelDisplay();
}
```

의미는 다음과 같다.

1. 현재 페이지가 `index.html`이 아니면 보호 페이지로 본다.
2. `requireAuth()`로 로그인 여부를 확인한다.
3. 로그인하지 않았으면 `index.html`로 보낸다.
4. 로그인 상태라면 사용자 이름/이메일, 알림, 내비게이션, 레벨 정보를 초기화한다.

빠른 구현에서는 이 구조를 유지한다. 단, `requireAuth()`가 보는 저장 데이터만 mock user에서 실제 로그인 응답으로 바뀐다.

### 3. 현재 인증 모듈

파일:

```text
frontend/js/modules/auth.js
```

현재 함수는 다음과 같다.

| 함수 | 현재 역할 | 실제 로그인 구현 시 변경 |
| --- | --- | --- |
| `login(email, password)` | 값 검증 후 mock user 저장 | `POST /api/auth/login` 호출 |
| `signup(...)` | 값 검증 후 mock user 저장 | 1차 구현에서는 유지 또는 준비중 처리 |
| `logout()` | localStorage user 제거 | 서버 logout 호출 후 localStorage 제거 |
| `checkAuth()` | `localStorage.user.loggedIn` 확인 | `authToken`과 `user` 확인 |
| `requireAuth()` | 미로그인 시 index 이동 | 기존 유지 |

현재 구조는 동기 함수다. `fetch()`를 쓰면 `login()`은 `async`가 되어야 한다. HTML에서 `app.login(...)` 호출 자체는 그대로 둬도 된다. 함수 내부가 알아서 API 요청 후 성공 시 이동하면 된다.

### 4. 상태 관리

파일:

```text
frontend/js/modules/state.js
```

현재 전역 상태:

```js
export const state = {
  user: null,
  ingredients: [],
  recipes: [],
  communityPosts: [],
  expiringItems: 0,
  level: 1,
  exp: 0
};
```

로그인에서 가장 중요한 것은 `state.user`다. `displayUserInfo()`가 이 값을 읽어 화면에 표시한다.

빠른 구현에서는 API 응답의 `member`를 다음 형태로 맞춰 `state.user`에 넣는다.

```js
{
  memberId: 1,
  email: "test@example.com",
  name: "테스트",
  role: "USER",
  status: "ACTIVE",
  level: 1,
  exp: 0,
  foodMbti: null,
  loggedIn: true
}
```

기존 프론트와 호환하려면 `name`, `email`, `loggedIn`은 반드시 있어야 한다.

### 5. localStorage 래퍼

파일:

```text
frontend/js/modules/storage.js
```

현재 `storage`는 JSON 직렬화/역직렬화만 담당한다.

빠른 구현에서도 이 래퍼를 그대로 쓴다.

권장 저장 key:

| key | 값 |
| --- | --- |
| `user` | 화면 표시용 member 객체 |
| `authToken` | 서버가 발급한 로그인 토큰 |

현재 다른 기능은 `userLevel`, `userExp`, `ingredients`, `communityPosts`도 localStorage에 저장한다. 로그인만 빠르게 붙일 때는 이 값을 바로 DB로 옮기지 않는다.

### 6. 사용자 정보 표시

파일:

```text
frontend/js/modules/profile.js
```

`displayUserInfo()`는 다음 DOM을 갱신한다.

| DOM id | 값 |
| --- | --- |
| `userName` | `state.user.name` |
| `userEmail` | `state.user.email` |

대부분의 보호 페이지 header는 이 id를 공통으로 가지고 있다.

즉 로그인 API 응답에서 `member.name`, `member.email`만 정확히 내려주면 기존 header는 그대로 동작한다.

### 7. CSS

파일:

```text
frontend/css/style.css
```

로그인 관련 CSS는 이미 충분하다.

| CSS class | 역할 |
| --- | --- |
| `.login-container` | 로그인 페이지 전체 배경과 중앙 정렬 |
| `.login-card` | 로그인 카드 폭 |
| `.tabs`, `.tabs-list`, `.tab-trigger`, `.tab-content` | 로그인/회원가입 탭 |
| `.form-input` | 이메일/비밀번호 입력 |
| `.btn`, `.btn-primary`, `.btn-success` | 로그인/회원가입 버튼 |
| `.toast`, `.toast.success`, `.toast.error` | 성공/실패 알림 |
| `.user-menu`, `.user-menu-dropdown` | 로그인 후 사용자 메뉴 |

따라서 빠른 구현에서는 CSS 변경이 거의 필요 없다. 필요한 것은 로딩 중 버튼 비활성화 스타일 정도다.

### 8. html-version 폴더

`frontend/html-version`에도 HTML/CSS/JS 복사본이 있다. 하지만 현재 모듈형 구조는 `frontend/pages`와 `frontend/js/modules`가 기준이다.

빠른 구현 기준은 다음으로 잡는다.

```text
frontend/pages/*
frontend/js/app.js
frontend/js/modules/*
frontend/css/style.css
```

`html-version`은 별도 배포용 또는 백업용이면 나중에 같은 변경을 반영한다.

## database_test 문서에서 가져올 설계 원칙

`docs/database_test` 문서의 핵심은 다음이다.

```text
main.cpp
  -> 설정 로드
  -> 라우터 연결
  -> 서버 실행

Controller
  -> HTTP 요청/응답 처리

Service
  -> 비즈니스 흐름 처리

Repository
  -> DB 작업 처리

Model
  -> DB row와 C++ 객체 매핑
```

로그인 기능에도 같은 흐름을 적용한다.

```text
POST /api/auth/login
  -> MemberController
    -> MemberService
      -> MemberRepository
        -> MemberModel / MemberSessionModel / MemberLoginHistoryModel
          -> MySQL
```

중요한 기준:

- `main.cpp`에 로그인 SQL을 쓰지 않는다.
- Controller는 request JSON을 읽고 response JSON을 만든다.
- Service는 이메일/비밀번호 검증과 계정 상태 판단을 한다.
- Repository는 DB 조회/저장만 한다.
- Model은 Drogon ORM Mapper가 사용할 DB row 구조를 가진다.
- DTO는 프론트와 주고받는 요청/응답 모양을 가진다.
- 테이블 생성은 요청마다 하지 말고 migration SQL로 관리한다.

## 1차 구현 범위

1차 구현은 다음만 한다.

| 기능 | 포함 여부 | 이유 |
| --- | --- | --- |
| 로그인 | 포함 | 지금 필요한 핵심 기능 |
| 로그인 상태 유지 | 포함 | 페이지 이동 후에도 로그인 상태 필요 |
| 로그아웃 | 포함 | 기존 user menu에 이미 있음 |
| 내 정보 표시 | 포함 | header와 profile 화면이 이미 사용 |
| 회원가입 | 제외 또는 mock 유지 | 로그인 빠른 구현이 우선 |
| 비밀번호 찾기 | 제외 | UI 링크만 있음 |
| 프로필 수정 | 제외 또는 mock 유지 | 현재 localStorage 기반으로 유지 가능 |
| 비밀번호 변경 | 제외 또는 mock 유지 | 로그인 이후 2차 구현 |

빠른 테스트를 위해 회원가입 없이 DB에 테스트 계정을 직접 넣어도 된다.

## 추천 API

### POST /api/auth/login

로그인 요청이다.

Request:

```json
{
  "email": "test@example.com",
  "password": "password1234",
  "rememberMe": true
}
```

Response success:

```json
{
  "ok": true,
  "message": "로그인 성공",
  "authToken": "server-generated-session-token",
  "member": {
    "memberId": 1,
    "email": "test@example.com",
    "name": "테스트",
    "role": "USER",
    "status": "ACTIVE",
    "level": 1,
    "exp": 0,
    "foodMbti": null,
    "lastLoginAt": "2026-05-11 02:00:00",
    "loggedIn": true
  }
}
```

Response failure:

```json
{
  "ok": false,
  "message": "이메일 또는 비밀번호가 올바르지 않습니다."
}
```

보안상 로그인 실패 시에는 `이메일이 없습니다`, `비밀번호가 틀렸습니다`처럼 세부 사유를 프론트에 그대로 알려주지 않는 편이 좋다. 내부 기록은 `member_login_history.failure_reason`에 남긴다.

### POST /api/auth/logout

로그아웃 요청이다.

Request header:

```text
Authorization: Bearer server-generated-session-token
```

Response:

```json
{
  "ok": true,
  "message": "로그아웃되었습니다."
}
```

프론트는 이 요청이 실패해도 localStorage의 `user`, `authToken`은 제거한다. 사용자가 로그아웃을 눌렀으면 브라우저 상태는 무조건 로그아웃으로 만드는 것이 UX상 자연스럽다.

### GET /api/auth/me

현재 로그인한 사용자를 확인하는 요청이다.

1차 빠른 구현에서는 생략 가능하다. 하지만 보호 페이지에 들어올 때 서버 세션까지 검증하려면 필요하다.

Request header:

```text
Authorization: Bearer server-generated-session-token
```

Response:

```json
{
  "ok": true,
  "member": {
    "memberId": 1,
    "email": "test@example.com",
    "name": "테스트",
    "role": "USER",
    "status": "ACTIVE",
    "level": 1,
    "exp": 0,
    "foodMbti": null,
    "loggedIn": true
  }
}
```

## DB 설계

`docs/DB_schema/login_schema.md`에 이미 설계가 있다. 로그인 빠른 구현에 필요한 테이블은 다음 3개다.

1. `members`
2. `member_sessions`
3. `member_login_history`

### members

회원의 핵심 정보다.

```sql
CREATE TABLE members (
  member_id BIGINT UNSIGNED NOT NULL AUTO_INCREMENT,
  email VARCHAR(255) NOT NULL,
  password_hash VARCHAR(255) NOT NULL,
  name VARCHAR(50) NOT NULL,
  role ENUM('USER', 'ADMIN') NOT NULL DEFAULT 'USER',
  status ENUM('ACTIVE', 'WITHDRAWN', 'SUSPENDED') NOT NULL DEFAULT 'ACTIVE',
  level INT UNSIGNED NOT NULL DEFAULT 1,
  exp INT UNSIGNED NOT NULL DEFAULT 0,
  food_mbti VARCHAR(20) NULL,
  created_at DATETIME NOT NULL DEFAULT CURRENT_TIMESTAMP,
  updated_at DATETIME NOT NULL DEFAULT CURRENT_TIMESTAMP ON UPDATE CURRENT_TIMESTAMP,
  last_login_at DATETIME NULL,

  PRIMARY KEY (member_id),
  UNIQUE KEY uq_members_email (email),
  KEY idx_members_status (status)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;
```

로그인할 때 가장 중요한 컬럼은 다음이다.

| 컬럼 | 역할 |
| --- | --- |
| `email` | 로그인 ID |
| `password_hash` | 비밀번호 검증용 해시 |
| `status` | 로그인 가능 상태인지 판단 |
| `last_login_at` | 마지막 로그인 시각 갱신 |
| `level`, `exp`, `food_mbti` | 프론트 프로필/헤더 표시용 |

`password_hash`는 절대 프론트 응답에 포함하지 않는다.

### member_sessions

로그인 유지용 세션이다.

```sql
CREATE TABLE member_sessions (
  session_id BIGINT UNSIGNED NOT NULL AUTO_INCREMENT,
  member_id BIGINT UNSIGNED NOT NULL,
  refresh_token_hash VARCHAR(255) NOT NULL,
  user_agent VARCHAR(255) NULL,
  ip_address VARCHAR(45) NULL,
  expires_at DATETIME NOT NULL,
  created_at DATETIME NOT NULL DEFAULT CURRENT_TIMESTAMP,
  revoked_at DATETIME NULL,

  PRIMARY KEY (session_id),
  UNIQUE KEY uq_member_sessions_refresh_token_hash (refresh_token_hash),
  KEY idx_member_sessions_member_id (member_id),
  KEY idx_member_sessions_expires_at (expires_at),

  CONSTRAINT fk_member_sessions_member
    FOREIGN KEY (member_id)
    REFERENCES members(member_id)
    ON DELETE CASCADE
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;
```

빠른 구현에서는 `authToken`을 발급하고, DB에는 그 원문이 아니라 해시를 저장한다.

프론트:

```text
localStorage.authToken = 원문 token
```

DB:

```text
member_sessions.refresh_token_hash = token 해시
```

### member_login_history

로그인 성공/실패 기록이다.

```sql
CREATE TABLE member_login_history (
  login_history_id BIGINT UNSIGNED NOT NULL AUTO_INCREMENT,
  member_id BIGINT UNSIGNED NULL,
  email VARCHAR(255) NOT NULL,
  success BOOLEAN NOT NULL,
  failure_reason VARCHAR(100) NULL,
  ip_address VARCHAR(45) NULL,
  user_agent VARCHAR(255) NULL,
  created_at DATETIME NOT NULL DEFAULT CURRENT_TIMESTAMP,

  PRIMARY KEY (login_history_id),
  KEY idx_login_history_member_id (member_id),
  KEY idx_login_history_email (email),
  KEY idx_login_history_created_at (created_at),

  CONSTRAINT fk_login_history_member
    FOREIGN KEY (member_id)
    REFERENCES members(member_id)
    ON DELETE SET NULL
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;
```

실패 기록을 남기면 나중에 보안 정책을 확장하기 쉽다.

예시:

- 같은 이메일로 5회 실패하면 잠시 제한
- 특정 IP에서 실패가 많으면 차단
- 관리자 화면에서 로그인 이력 조회

## 백엔드 파일 구조

`database_test` 문서의 권장 구조를 `Member` 로그인에 적용하면 다음과 같다.

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
    orm/
      MemberModel.h
      MemberSessionModel.h
      MemberLoginHistoryModel.h
    dto/
      MemberDTO.h
      LoginRequestDTO.h
      LoginResponseDTO.h
  security/
    PasswordHasher.h
    TokenGenerator.h
  database/
    migrations/
      001_create_member_login_tables.sql
```

현재 `backend`에는 `repositories` 폴더가 없다. 실제 구현 시 추가하는 것을 권장한다.

빠르게 구현해야 해서 파일 수를 줄이고 싶다면 1차에서는 이렇게 시작할 수 있다.

```text
backend/
  controllers/
    MemberController.cpp
  services/
    MemberService.cpp
  repositories/
    MemberRepository.h
  mappers/
    MemberMapper.cpp
  models/
    MemberDTO.cpp
    MemberModel.h
    MemberSessionModel.h
    MemberLoginHistoryModel.h
```

하지만 장기적으로는 `models/orm`, `models/dto`를 나누는 편이 좋다.

## 백엔드 계층별 상세 역할

### MemberController

HTTP 요청과 응답만 담당한다.

담당:

- `POST /api/auth/login`
- `POST /api/auth/logout`
- `GET /api/auth/me`
- JSON body 읽기
- 필수 필드가 비어 있는지 1차 확인
- `MemberService` 호출
- 성공/실패 JSON 응답 생성

하지 말 것:

- SQL 직접 실행
- 비밀번호 해시 직접 비교
- DB 모델을 그대로 프론트에 반환
- 로그인 성공 여부의 세부 비즈니스 판단

객체 이름 규칙 예시:

```cpp
service::MemberService memberService(memberRepository);
dto::LoginRequestDTO loginRequestDTO(requestJson);
dto::LoginResponseDTO loginResponseDTO =
    memberService.login(loginRequestDTO);
```

### MemberService

로그인 비즈니스 흐름을 담당한다.

담당:

- 이메일 형식과 비밀번호 입력 여부 확인
- `MemberRepository.findByEmail(email)` 호출
- 회원이 존재하는지 확인
- `status == ACTIVE`인지 확인
- 비밀번호 해시 비교
- 로그인 성공 시 세션 생성
- 로그인 성공 시 `last_login_at` 갱신
- 로그인 성공/실패 이력 저장
- `LoginResponseDTO` 반환

로그인 서비스 흐름:

```text
MemberService.login(loginRequestDTO)
  1. email/password 비어 있는지 확인
  2. memberRepository.findByEmail(email)
  3. 없으면 login history 실패 기록
  4. status가 ACTIVE가 아니면 실패
  5. passwordHasher.verify(password, passwordHash)
  6. 실패하면 login history 실패 기록
  7. 성공하면 tokenGenerator.generate()
  8. token hash를 member_sessions에 저장
  9. members.last_login_at 갱신
  10. MemberMapper.toMemberDTO(memberModel)
  11. LoginResponseDTO 반환
```

### MemberRepository

DB 작업만 담당한다.

필요 메서드:

```text
findByEmail(email)
findById(memberId)
updateLastLoginAt(memberId)
createSession(memberId, tokenHash, expiresAt, ipAddress, userAgent)
findSessionByTokenHash(tokenHash)
revokeSessionByTokenHash(tokenHash)
insertLoginHistory(memberId, email, success, failureReason, ipAddress, userAgent)
```

Drogon ORM `Mapper<T>`를 사용한다면 `database_test`의 `HealthCheckRepository`처럼 DB client를 생성자에서 주입받는다.

객체 이름 규칙 예시:

```cpp
repository::MemberRepository memberRepository(
    drogon::app().getDbClient("default"));
```

### MemberMapper

DB 모델을 응답 DTO로 바꾸는 변환 계층이다.

필요 메서드:

```text
toMemberDTO(memberModel)
toLoginResponseDTO(memberDTO, authToken)
```

중요한 점:

- `MemberModel.passwordHash()`는 DTO에 넣지 않는다.
- `MemberSessionModel.refreshTokenHash()`도 DTO에 넣지 않는다.
- 프론트가 필요한 `memberId`, `email`, `name`, `role`, `status`, `level`, `exp`, `foodMbti`, `lastLoginAt`만 내려준다.

### MemberModel

`members` 테이블 row를 나타내는 Drogon ORM 모델이다.

`database_test`의 `HealthCheckModel`과 같은 방식으로 다음을 제공한다.

```text
using PrimaryKeyType
tableName
primaryKeyName
MemberModel(const drogon::orm::Row &row)
sqlForFindingByPrimaryKey()
getColumnName(index)
sqlForInserting(needSelection)
outputArgs(binder)
updateColumns()
updateArgs(binder)
getPrimaryKey()
updateId(id)
```

필드 예시:

```text
memberId_
email_
passwordHash_
name_
role_
status_
level_
exp_
foodMbti_
createdAt_
updatedAt_
lastLoginAt_
```

응답 DTO에는 `passwordHash_`를 절대 포함하지 않는다.

### MemberSessionModel

`member_sessions` 테이블 row를 나타낸다.

주요 필드:

```text
sessionId_
memberId_
refreshTokenHash_
userAgent_
ipAddress_
expiresAt_
createdAt_
revokedAt_
```

로그인 성공 시 새 row를 insert한다.

로그아웃 시 `revokedAt_`을 현재 시간으로 갱신한다.

### MemberLoginHistoryModel

`member_login_history` 테이블 row를 나타낸다.

주요 필드:

```text
loginHistoryId_
memberId_
email_
success_
failureReason_
ipAddress_
userAgent_
createdAt_
```

로그인 성공/실패 모두 기록한다.

## DTO 설계

### LoginRequestDTO

프론트 로그인 form에서 오는 요청이다.

```cpp
class LoginRequestDTO {
public:
  std::string email;
  std::string password;
  bool rememberMe;
};
```

객체 이름:

```cpp
LoginRequestDTO loginRequestDTO;
```

### MemberDTO

프론트에 내려줄 회원 정보다. 현재 `backend/models/MemberDTO.cpp`의 구조를 기반으로 한다.

```cpp
class MemberDTO {
public:
  unsigned long long memberId;
  std::string email;
  std::string name;
  std::string role;
  std::string status;
  unsigned int level;
  unsigned int exp;
  std::optional<std::string> foodMbti;
  std::string createdAt;
  std::string updatedAt;
  std::optional<std::string> lastLoginAt;
};
```

객체 이름:

```cpp
MemberDTO memberDTO;
```

### LoginResponseDTO

로그인 성공 응답이다.

```cpp
class LoginResponseDTO {
public:
  bool ok;
  std::string message;
  std::string authToken;
  MemberDTO memberDTO;
};
```

객체 이름:

```cpp
LoginResponseDTO loginResponseDTO;
```

JSON으로 내려갈 때는 `memberDTO`라는 이름을 그대로 노출하기보다 프론트가 쓰기 쉬운 `member` key로 내려준다.

```json
{
  "ok": true,
  "message": "로그인 성공",
  "authToken": "...",
  "member": {
    "memberId": 1,
    "email": "test@example.com",
    "name": "테스트"
  }
}
```

## 프론트엔드 변경 설계

### 1. api.js 추가

새 파일:

```text
frontend/js/modules/api.js
```

역할:

- API base URL 관리
- JSON request 공통 처리
- `Authorization` header 자동 추가
- 서버 오류 메시지 표준화

예상 형태:

```js
import { storage } from './storage.js';

const API_BASE_URL = 'http://127.0.0.1:8080';

export async function request(path, options = {}) {
  const authToken = storage.get('authToken');

  const headers = {
    'Content-Type': 'application/json',
    ...(options.headers || {})
  };

  if (authToken) {
    headers.Authorization = `Bearer ${authToken}`;
  }

  const response = await fetch(`${API_BASE_URL}${path}`, {
    ...options,
    headers
  });

  const data = await response.json();

  if (!response.ok || data.ok === false) {
    throw new Error(data.message || '요청 처리 중 오류가 발생했습니다.');
  }

  return data;
}
```

### 2. auth.js login() 변경

현재 mock login은 서버 호출로 바꾼다.

현재:

```js
export function login(email, password) {
  ...
  const user = {
    email: email,
    name: email.split('@')[0],
    loggedIn: true
  };

  storage.set('user', user);
  state.user = user;
  ...
}
```

변경 후 흐름:

```text
login(email, password)
  1. 입력값 검증
  2. rememberMe 읽기
  3. POST /api/auth/login 호출
  4. response.member에 loggedIn = true 추가
  5. storage.set('authToken', response.authToken)
  6. storage.set('user', member)
  7. state.user = member
  8. level/exp localStorage와 동기화
  9. home.html 이동
```

권장 구현 형태:

```js
export async function login(email, password) {
  if (!email || !password) {
    showToast('이메일과 비밀번호를 입력해주세요.', 'error');
    return false;
  }

  try {
    const rememberMe = document.getElementById('rememberMe')?.checked ?? false;

    const data = await request('/api/auth/login', {
      method: 'POST',
      body: JSON.stringify({ email, password, rememberMe })
    });

    const user = {
      ...data.member,
      loggedIn: true
    };

    storage.set('authToken', data.authToken);
    storage.set('user', user);
    storage.set('userLevel', user.level || 1);
    storage.set('userExp', user.exp || 0);
    state.user = user;

    showToast('로그인 성공!', 'success');

    setTimeout(() => {
      window.location.href = 'home.html';
    }, 500);

    return true;
  } catch (error) {
    showToast(error.message, 'error');
    return false;
  }
}
```

### 3. auth.js checkAuth() 변경

빠른 구현에서는 localStorage 기반 확인을 유지한다.

```js
export function checkAuth() {
  const user = storage.get('user');
  const authToken = storage.get('authToken');

  if (user && user.loggedIn && authToken) {
    state.user = user;
    return true;
  }

  return false;
}
```

나중에는 `GET /api/auth/me`를 호출해서 서버 세션까지 확인하는 방식으로 바꾼다.

### 4. auth.js logout() 변경

로그아웃은 서버에 알리고, localStorage를 정리한다.

```text
logout()
  1. POST /api/auth/logout 호출 시도
  2. 성공/실패와 관계없이 localStorage 정리
  3. state.user = null
  4. index.html 이동
```

정리할 key:

```text
user
authToken
```

`userLevel`, `userExp`는 계정별 데이터로 볼 수 있으므로 로그아웃 때 제거하는 편이 안전하다. 다만 현재 다른 기능이 localStorage에 강하게 의존하므로, 1차 구현에서는 `userLevel`, `userExp` 제거 여부를 선택해야 한다.

권장:

```text
로그아웃 시 user, authToken, userLevel, userExp 제거
```

### 5. index.html 변경

로그인 유지 체크박스에 id를 추가한다.

```html
<input type="checkbox" id="rememberMe">
```

버튼 로딩 처리는 1차에서는 생략 가능하다. 여유가 있으면 로그인 버튼에 `id="loginButton"`을 추가해서 API 요청 중 비활성화한다.

### 6. profile.js는 1차에서 유지

`displayUserInfo()`는 그대로 사용한다.

`updateProfile()`, `changePassword()`는 현재 mock이다. 로그인만 빠르게 붙일 때는 건드리지 않는다.

다만 문서상으로는 다음 2차 API와 연결하면 된다.

```text
PATCH /api/members/me
PATCH /api/members/me/password
```

## 백엔드 로그인 상세 흐름

### 성공 흐름

```text
1. 브라우저에서 index.html 로그인 form submit
2. auth.js login()이 POST /api/auth/login 호출
3. MemberController가 JSON body를 읽어 LoginRequestDTO 생성
4. MemberService.login(loginRequestDTO) 호출
5. MemberRepository.findByEmail(email)
6. MemberModel 생성
7. MemberService가 status 확인
8. MemberService가 passwordHasher.verify(password, passwordHash) 실행
9. tokenGenerator.generate()로 authToken 생성
10. token hash 생성
11. MemberRepository.createSession(...)
12. MemberRepository.updateLastLoginAt(memberId)
13. MemberRepository.insertLoginHistory(success=true)
14. MemberMapper.toMemberDTO(memberModel)
15. LoginResponseDTO 생성
16. Controller가 JSON 응답 반환
17. auth.js가 authToken과 user를 localStorage에 저장
18. home.html로 이동
19. app.js가 requireAuth() 통과
20. displayUserInfo()가 header에 이름/이메일 표시
```

### 실패 흐름

```text
1. 로그인 요청 도착
2. email로 회원 조회
3. 회원 없음 또는 비밀번호 불일치 또는 status 비정상
4. MemberRepository.insertLoginHistory(success=false)
5. Controller가 401 응답
6. auth.js가 toast error 표시
7. 페이지 이동 없음
```

프론트 표시 메시지는 하나로 통일한다.

```text
이메일 또는 비밀번호가 올바르지 않습니다.
```

서버 내부 `failure_reason`은 더 자세히 남겨도 된다.

```text
MEMBER_NOT_FOUND
PASSWORD_MISMATCH
STATUS_WITHDRAWN
STATUS_SUSPENDED
```

## C++ 코드 작성 시 클래스별 객체명 예시

### Controller

```cpp
void MemberController::login(
    const drogon::HttpRequestPtr &httpRequestPtr,
    std::function<void(const drogon::HttpResponsePtr &)> &&callback)
{
    dto::LoginRequestDTO loginRequestDTO(httpRequestPtr->getJsonObject());

    repository::MemberRepository memberRepository(
        drogon::app().getDbClient("default"));

    service::MemberService memberService(memberRepository);

    dto::LoginResponseDTO loginResponseDTO =
        memberService.login(loginRequestDTO);

    callback(makeJsonResponse(loginResponseDTO.toJson()));
}
```

객체명 확인:

| 클래스 | 객체명 |
| --- | --- |
| `LoginRequestDTO` | `loginRequestDTO` |
| `MemberRepository` | `memberRepository` |
| `MemberService` | `memberService` |
| `LoginResponseDTO` | `loginResponseDTO` |

### Service

```cpp
dto::LoginResponseDTO MemberService::login(
    const dto::LoginRequestDTO &loginRequestDTO)
{
    model::MemberModel memberModel =
        memberRepository_.findByEmail(loginRequestDTO.email);

    security::PasswordHasher passwordHasher;
    const bool passwordMatches =
        passwordHasher.verify(loginRequestDTO.password,
                              memberModel.passwordHash());

    if (!passwordMatches)
    {
        memberRepository_.insertLoginHistory(...);
        throw LoginFailedException();
    }

    security::TokenGenerator tokenGenerator;
    const std::string authToken = tokenGenerator.generate();

    model::MemberSessionModel memberSessionModel(
        memberModel.memberId(),
        hashToken(authToken),
        calculateExpiresAt(loginRequestDTO.rememberMe));

    memberRepository_.createSession(memberSessionModel);

    mapper::MemberMapper memberMapper;
    dto::MemberDTO memberDTO = memberMapper.toMemberDTO(memberModel);

    return dto::LoginResponseDTO("로그인 성공", authToken, memberDTO);
}
```

객체명 확인:

| 클래스 | 객체명 |
| --- | --- |
| `MemberModel` | `memberModel` |
| `PasswordHasher` | `passwordHasher` |
| `TokenGenerator` | `tokenGenerator` |
| `MemberSessionModel` | `memberSessionModel` |
| `MemberMapper` | `memberMapper` |
| `MemberDTO` | `memberDTO` |

## 빠른 구현 순서

### 1단계: DB 준비

1. `docs/DB_schema/login_schema.md`의 `members`, `member_sessions`, `member_login_history` 생성 SQL을 migration으로 저장한다.
2. 테스트 계정을 하나 만든다.
3. 비밀번호는 평문이 아니라 해시로 저장한다.

개발 초기에 해시 도구가 아직 없으면 임시로만 평문 비교를 넣을 수 있지만, 문서 기준 설계는 반드시 해시 저장이다. 실제 커밋 전에는 해시 비교로 바꿔야 한다.

### 2단계: Backend Model 작성

`database_test`의 `HealthCheckModel`을 참고해서 `MemberModel`을 만든다.

필수:

- `tableName = "members"`
- `primaryKeyName = "member_id"`
- `sqlForFindingByPrimaryKey()`
- `sqlForFindingByEmail()`은 Drogon Mapper 기본 계약이 아니므로 Repository에서 별도 SQL 또는 custom query로 처리
- `Row` 생성자에서 DB 컬럼을 C++ 필드로 변환
- `password_hash` getter는 Service 내부에서만 사용

### 3단계: Repository 작성

`MemberRepository`는 `DbClientPtr`을 생성자로 받는다.

```cpp
explicit MemberRepository(drogon::orm::DbClientPtr dbClient)
    : dbClient_(std::move(dbClient))
{
}
```

필수 메서드:

```text
findByEmail(email)
updateLastLoginAt(memberId)
createSession(memberSessionModel)
insertLoginHistory(memberLoginHistoryModel)
```

### 4단계: Service 작성

`MemberService`는 로그인 흐름을 가진다.

필수 검증:

- email 비어 있음
- password 비어 있음
- 회원 없음
- `status != ACTIVE`
- password mismatch

성공 시:

- 세션 생성
- 마지막 로그인 시각 갱신
- 성공 이력 저장
- `LoginResponseDTO` 반환

### 5단계: Controller 작성

`MemberController`는 다음 endpoint를 등록한다.

```text
POST /api/auth/login
POST /api/auth/logout
GET  /api/auth/me
```

빠른 구현에서는 `/api/auth/login`부터 만든다.

### 6단계: Frontend api.js 추가

`frontend/js/modules/api.js`를 만들고 `request()` helper를 둔다.

### 7단계: auth.js 교체

`login()`, `checkAuth()`, `logout()`을 API 기반으로 바꾼다.

### 8단계: index.html checkbox id 추가

`로그인 유지` checkbox에 `id="rememberMe"`를 추가한다.

### 9단계: 테스트

브라우저에서 다음을 확인한다.

1. 잘못된 이메일/비밀번호로 로그인하면 toast error가 뜬다.
2. 올바른 계정으로 로그인하면 home으로 이동한다.
3. home header에 이름과 이메일이 표시된다.
4. 새로고침해도 로그인 상태가 유지된다.
5. 로그아웃하면 index로 이동한다.
6. 로그아웃 후 home.html에 직접 접근하면 index로 이동한다.

## 1차 구현에서 피해야 할 것

- 로그인 기능을 만들면서 재료, 레시피, 커뮤니티 데이터를 전부 DB로 옮기지 않는다.
- 회원가입까지 무리해서 같이 끝내려고 하지 않는다.
- `main.cpp`에 모든 로그인 로직을 넣지 않는다.
- `MemberDTO`에 `passwordHash`를 넣지 않는다.
- 프론트 localStorage에 비밀번호를 저장하지 않는다.
- 실패 사유를 프론트에 너무 자세히 알려주지 않는다.
- 객체명 규칙을 어기지 않는다.

## 최종 추천 구조

로그인 1차 구현의 최종 흐름은 다음으로 고정한다.

```text
frontend/pages/index.html
  -> app.login()
    -> auth.js login()
      -> api.js request('/api/auth/login')
        -> backend MemberController
          -> MemberService
            -> MemberRepository
              -> MemberModel / MemberSessionModel / MemberLoginHistoryModel
                -> MySQL
```

로그인 성공 후:

```text
LoginResponseDTO
  -> JSON { ok, message, authToken, member }
    -> auth.js
      -> storage.set('authToken')
      -> storage.set('user')
      -> state.user
      -> home.html
      -> displayUserInfo()
```

이 방식이면 현재 프론트 UI를 거의 그대로 유지하면서 `database_test`에서 정리한 Drogon DB 접근 흐름을 실제 `Member` 로그인에 적용할 수 있다.

