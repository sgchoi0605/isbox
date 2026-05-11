# frontend/html-version 로그인 최소 구현 계획

## 목적

이 문서는 `frontend/html-version` 기준으로 로그인 기능만 실제 DB 기반 흐름에 연결하는 방법을 정리한다.

이번 문서의 DB 관심사는 `members` 테이블이다. 재료, 냉장고, 커뮤니티, 레시피, MBTI 저장 테이블은 이 문서에서 설계하지 않는다. 해당 데이터는 별도 테이블과 API를 구성한 뒤 필요한 곳에서 참조한다.

## 범위

포함:

```text
- DB에 저장된 회원 이메일/비밀번호로 로그인
- 로그인 성공 후 회원 정보를 프론트 state에 반영
- 보호 페이지에서 현재 로그인 회원 확인
- 로그아웃 처리
- members.last_login_at 갱신
```

제외:

```text
- 재료 테이블 설계
- /api/ingredients 설계
- 재료 localStorage 마이그레이션
- 커뮤니티/게시글 테이블 설계
- 레시피/MBTI 저장 테이블 설계
- 회원가입 DB 연결
- 비밀번호 찾기/변경
- 프로필 수정
- 로그인 이력 테이블
```

중요한 기준:

```text
로그인 작업에서는 localStorage.user만 대체한다.
ingredients, communityPosts 같은 다른 localStorage 데이터는 이번 작업에서 건드리지 않는다.
storage 객체 전체 삭제도 이번 작업에 포함하지 않는다.
```

## 대상 구조

현재 프론트 대상은 다음 구조다.

```text
frontend/html-version/
  index.html
  home.html
  profile.html
  ingredients.html
  recipes.html
  community.html
  food-mbti.html
  css/
    style.css
  js/
    app.js
```

로그인 구현에서 주로 보는 파일은 다음이다.

```text
frontend/html-version/index.html
frontend/html-version/home.html
frontend/html-version/profile.html
frontend/html-version/js/app.js
backend/controllers/MemberController.cpp
backend/services/MemberService.cpp
backend/mappers/MemberMapper.cpp
backend/models/MemberDTO.cpp
```

전체 흐름은 다음처럼 고정한다.

```text
회원
  -> View(UI: index.html)
    -> app.login(email, password)
      -> MemberController
        -> MemberService
          -> MemberMapper
            -> DB: members
```

## DB 범위

이번 문서에서 직접 설계하고 조회하는 테이블은 `members`다.

필요 컬럼 예시는 다음 정도면 충분하다.

```sql
CREATE TABLE members (
  member_id BIGINT UNSIGNED NOT NULL AUTO_INCREMENT,
  email VARCHAR(255) NOT NULL,
  password_hash VARCHAR(255) NOT NULL,
  name VARCHAR(100) NOT NULL,
  role VARCHAR(20) NOT NULL DEFAULT 'USER',
  status VARCHAR(20) NOT NULL DEFAULT 'ACTIVE',
  level INT UNSIGNED NOT NULL DEFAULT 1,
  exp INT UNSIGNED NOT NULL DEFAULT 0,
  last_login_at DATETIME NULL,
  created_at DATETIME NOT NULL DEFAULT CURRENT_TIMESTAMP,
  updated_at DATETIME NOT NULL DEFAULT CURRENT_TIMESTAMP ON UPDATE CURRENT_TIMESTAMP,

  PRIMARY KEY (member_id),
  UNIQUE KEY uq_members_email (email),
  KEY idx_members_status (status)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;
```

빠른 연결 테스트용 계정 예시:

```sql
INSERT INTO members (
  email,
  password_hash,
  name,
  role,
  status,
  level,
  exp
) VALUES (
  'test@example.com',
  'password1234',
  '테스트',
  'USER',
  'ACTIVE',
  1,
  0
);
```

위 예시는 연결 테스트용이다. 최종 구현에서는 `password_hash`에 평문이 아니라 bcrypt, Argon2, PBKDF2 같은 해시 값을 저장해야 한다.

로그인 상태 유지를 위한 쿠키/세션 저장 방식은 인증 공통 영역에서 결정한다. 이 문서에서는 별도 세션 테이블 DDL을 작성하지 않는다.

## API

로그인 최소 API는 다음 세 개다.

```text
POST /api/auth/login
GET  /api/members/me
POST /api/auth/logout
```

### POST /api/auth/login

요청:

```json
{
  "email": "test@example.com",
  "password": "password1234"
}
```

성공 응답:

```json
{
  "ok": true,
  "message": "로그인 성공",
  "member": {
    "memberId": 1,
    "email": "test@example.com",
    "name": "테스트",
    "role": "USER",
    "status": "ACTIVE",
    "level": 1,
    "exp": 0,
    "loggedIn": true
  }
}
```

실패 응답:

```json
{
  "ok": false,
  "message": "이메일 또는 비밀번호가 올바르지 않습니다."
}
```

프론트에는 회원 없음, 비밀번호 불일치, 비활성 계정 같은 세부 사유를 구분해서 내려주지 않는다. 내부 로그나 내부 코드에서만 구분한다.

### GET /api/members/me

현재 로그인한 회원 정보를 가져온다.

성공 응답:

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
    "loggedIn": true
  }
}
```

로그인 상태가 아니면 `401`과 함께 실패 응답을 반환한다.

### POST /api/auth/logout

현재 로그인 상태를 만료한다.

응답:

```json
{
  "ok": true,
  "message": "로그아웃되었습니다."
}
```

## Backend 설계

### DTO

`backend/models/MemberDTO.cpp`에는 로그인에 필요한 데이터 구조만 둔다.

```cpp
class LoginRequestDTO {
public:
  string email;
  string password;
};
```

```cpp
class MemberModel {
public:
  unsigned long long memberId;
  string email;
  string passwordHash;
  string name;
  string role;
  string status;
  unsigned int level;
  unsigned int exp;
  optional<string> lastLoginAt;
};
```

```cpp
class MemberDTO {
public:
  unsigned long long memberId;
  string email;
  string name;
  string role;
  string status;
  unsigned int level;
  unsigned int exp;
  optional<string> lastLoginAt;
};
```

```cpp
class LoginResponseDTO {
public:
  bool ok;
  string message;
  optional<MemberDTO> memberDTO;
};
```

기준:

```text
MemberModel: DB row 표현. passwordHash 포함 가능.
MemberDTO: 프론트 응답. passwordHash 포함 금지.
LoginRequestDTO: 프론트 요청.
LoginResponseDTO: 로그인 처리 결과.
```

### Mapper

`backend/mappers/MemberMapper.cpp`는 `members` 테이블만 다룬다.

필요 함수:

```text
findByEmail(email)
findById(memberId)
updateLastLoginAt(memberId)
toMemberDTO(memberModel)
```

`findByEmail(email)`:

```sql
SELECT
  member_id,
  email,
  password_hash,
  name,
  role,
  status,
  level,
  exp,
  last_login_at
FROM members
WHERE email = ?
```

`updateLastLoginAt(memberId)`:

```sql
UPDATE members
SET last_login_at = NOW()
WHERE member_id = ?
```

`toMemberDTO(memberModel)`은 `passwordHash`를 버리고 프론트에 내려줄 필드만 담는다.

### Service

`backend/services/MemberService.cpp`는 로그인 가능 여부를 판단한다.

필요 함수:

```text
login(loginRequestDTO)
getCurrentMember(memberId)
logout()
```

`login(loginRequestDTO)` 처리 순서:

```text
1. email 비어 있으면 실패
2. password 비어 있으면 실패
3. MemberMapper.findByEmail(email)
4. 회원이 없으면 실패
5. status가 ACTIVE가 아니면 실패
6. 입력 password와 memberModel.passwordHash 검증
7. 실패면 공통 실패 메시지 반환
8. 성공이면 updateLastLoginAt(memberId)
9. MemberDTO 생성
10. LoginResponseDTO 반환
```

비밀번호 검증 기준:

```text
개발 연결 테스트: password == passwordHash 허용 가능
최종 구현: passwordHasher.verify(password, passwordHash)
```

최종 구현에는 평문 비밀번호 비교를 남기지 않는다.

### Controller

`backend/controllers/MemberController.cpp`는 HTTP 요청/응답만 담당한다.

필요 endpoint:

```text
POST /api/auth/login
GET  /api/members/me
POST /api/auth/logout
```

Controller가 하는 일:

```text
1. JSON body 확인
2. email/password 읽기
3. LoginRequestDTO 생성
4. MemberService 호출
5. Service 결과를 JSON으로 변환
6. 로그인 성공이면 인증 쿠키 또는 기존 세션을 설정
7. 실패면 400 또는 401 반환
```

Controller가 하지 않는 일:

```text
- SQL 작성
- DB row 직접 해석
- 비밀번호 검증
- 회원 status 판단
- MemberModel을 그대로 JSON 반환
```

## Frontend 변경

### index.html

현재 form은 `app.login(email, password)`를 호출하므로 큰 변경이 필요 없다.

```html
<form onsubmit="event.preventDefault(); app.login(document.getElementById('loginEmail').value, document.getElementById('loginPassword').value);">
```

로그인 유지 체크박스는 이번 구현에서 사용하지 않는다.

### app.js

`frontend/html-version/js/app.js`의 로그인 mock을 API 호출로 바꾼다.

```js
const API_BASE_URL = 'http://127.0.0.1:8080';
```

```js
async function login(email, password) {
  if (!email || !password) {
    showToast('이메일과 비밀번호를 입력해주세요.', 'error');
    return false;
  }

  try {
    const response = await fetch(`${API_BASE_URL}/api/auth/login`, {
      method: 'POST',
      credentials: 'include',
      headers: {
        'Content-Type': 'application/json'
      },
      body: JSON.stringify({ email, password })
    });

    const data = await response.json();

    if (!response.ok || data.ok === false) {
      throw new Error(data.message || '로그인에 실패했습니다.');
    }

    state.user = {
      ...data.member,
      loggedIn: true
    };

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

보호 페이지 확인은 `localStorage.user`가 아니라 `/api/members/me`로 한다.

```js
async function checkAuth() {
  const response = await fetch(`${API_BASE_URL}/api/members/me`, {
    credentials: 'include'
  });

  if (!response.ok) {
    return false;
  }

  const data = await response.json();
  if (data.ok === false) {
    return false;
  }

  state.user = {
    ...data.member,
    loggedIn: true
  };

  return true;
}
```

```js
async function requireAuth() {
  if (!(await checkAuth())) {
    window.location.href = 'index.html';
    return false;
  }
  return true;
}
```

로그아웃은 서버에 요청한 뒤 화면 state만 비운다.

```js
async function logout() {
  await fetch(`${API_BASE_URL}/api/auth/logout`, {
    method: 'POST',
    credentials: 'include'
  }).catch(() => null);

  state.user = null;
  showToast('로그아웃되었습니다.', 'success');

  setTimeout(() => {
    window.location.href = 'index.html';
  }, 500);
}
```

주의:

```text
- storage.set('user', user)는 제거한다.
- storage.get('user') 기반 로그인 판정은 제거한다.
- storage.get('ingredients') 같은 재료 관련 코드는 이번 작업에서 바꾸지 않는다.
- loadIngredients(), addIngredient(), deleteIngredient()는 별도 재료 테이블/API 작업에서 다룬다.
```

## 구현 순서

1. `members` 테이블과 테스트 계정을 준비한다.
2. `MemberDTO.cpp`에 로그인 요청/응답/회원 모델 구조를 정리한다.
3. `MemberMapper.cpp`에 `findByEmail`, `findById`, `updateLastLoginAt`, `toMemberDTO`를 구현한다.
4. `MemberService.cpp`에 로그인 검증 로직을 구현한다.
5. `MemberController.cpp`에 `POST /api/auth/login`, `GET /api/members/me`, `POST /api/auth/logout`을 연결한다.
6. `frontend/html-version/js/app.js`에서 `login`, `checkAuth`, `requireAuth`, `logout`만 API 기반으로 바꾼다.
7. 재료/커뮤니티/MBTI 관련 함수는 건드리지 않는다.

## 테스트 체크리스트

```text
1. 빈 이메일/비밀번호로 로그인하면 에러 toast가 뜬다.
2. DB에 없는 이메일로 로그인하면 공통 실패 메시지가 뜬다.
3. 틀린 비밀번호로 로그인하면 공통 실패 메시지가 뜬다.
4. status가 ACTIVE가 아닌 회원은 로그인되지 않는다.
5. 정상 회원은 로그인 후 home.html로 이동한다.
6. home.html header에 members.name, members.email이 표시된다.
7. 새로고침 후 /api/members/me로 로그인 회원을 다시 가져온다.
8. 로그아웃하면 index.html로 이동한다.
9. 로그아웃 후 보호 페이지에 직접 접근하면 index.html로 이동한다.
10. 재료 목록 기능은 이번 로그인 변경으로 동작 방식이 바뀌지 않는다.
```

## 최종 정리

이번 로그인 구현의 핵심은 `members` 테이블 기준으로 회원을 조회하고, `frontend/html-version/js/app.js`의 mock login을 실제 API 호출로 바꾸는 것이다.

재료 테이블과 재료 API는 이 문서의 범위가 아니다. 로그인 구현 중에는 `localStorage.user`만 DB/API 기반으로 대체하고, 재료 데이터는 별도 구성 후 참조한다.
