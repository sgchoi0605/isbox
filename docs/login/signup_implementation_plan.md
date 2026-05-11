# frontend/html-version 기준 회원가입 구현 설명서

## 목적

이 문서는 `frontend/html-version`의 현재 로그인/회원가입 화면을 기준으로 회원가입 기능을 실제 DB 기반 기능으로 구성하는 방법을 설명한다.

현재 대상 구조는 다음이다.

```text
frontend/html-version/
  index.html
  home.html
  profile.html
  css/style.css
  js/app.js

backend/controllers/MemberController.cpp
backend/services/MemberService.cpp
backend/mappers/MemberMapper.cpp
backend/models/MemberDTO.cpp
docs/DB_schema/login_schema.md
```

회원가입도 로그인과 같은 흐름을 따른다.

```text
회원
  -> View(UI)
    -> Controller
      -> Service
        -> Mapper
          -> DB
```

`MemberDTO`, `SignupRequestDTO`, `SignupResponseDTO`는 흐름의 단계가 아니라 각 단계 사이에서 데이터를 담는 구조로 사용한다.

## 가장 중요한 결론

현재 `index.html`에는 이미 회원가입 탭이 있고, submit 시 다음 함수를 호출한다.

```html
app.signup(
  document.getElementById('signupName').value,
  document.getElementById('signupEmail').value,
  document.getElementById('signupPassword').value,
  document.getElementById('signupConfirmPassword').value
)
```

따라서 1차 구현에서는 HTML 구조를 크게 바꾸지 않고 `frontend/html-version/js/app.js`의 `signup()` 내부만 서버 API 호출 방식으로 바꾸는 것이 좋다.

추천 1차 범위:

```text
포함:
- 이름, 이메일, 비밀번호, 비밀번호 확인으로 회원 생성
- 이메일 중복 방지
- 비밀번호 해시 저장
- 기본 role/status/level/exp 설정
- 회원가입 성공 시 자동 로그인 세션 생성
- 성공 후 home.html 이동

제외:
- 이메일 인증
- 비밀번호 찾기
- 소셜 로그인
- 약관 동의 이력 DB 저장
- 가입 완료 이메일 발송
- 프로필 상세 입력
```

회원가입 성공 후 자동 로그인하는 방식을 추천한다. 현재 mock `signup()`도 `state.user`를 만들고 바로 `home.html`로 이동하기 때문에, 기존 UX와 가장 잘 맞는다.

## 현재 html-version 회원가입 구조

### index.html

회원가입 탭은 `frontend/html-version/index.html` 안에 있다.

현재 입력 필드는 다음이다.

| DOM id | 의미 | 백엔드 필드 |
| --- | --- | --- |
| `signupName` | 이름 | `name` |
| `signupEmail` | 이메일 | `email` |
| `signupPassword` | 비밀번호 | `password` |
| `signupConfirmPassword` | 비밀번호 확인 | `confirmPassword` |
| 이름 없는 checkbox | 약관 동의 | `agreeTerms` |

약관 체크박스는 현재 `required`만 있고 `id`가 없다. 서버 요청에도 동의 여부를 포함하려면 다음처럼 id를 추가한다.

```html
<input type="checkbox" id="signupAgreeTerms" required style="margin-top: 0.25rem;">
```

기존 inline submit은 그대로 둘 수 있다. `signup()` 함수 내부에서 `signupAgreeTerms`를 읽으면 HTML 호출부를 바꾸지 않아도 된다.

### app.js

현재 `frontend/html-version/js/app.js`의 `signup()`은 DB를 호출하지 않고 localStorage에 user를 저장한다.

```js
function signup(name, email, password, confirmPassword) {
  if (!name || !email || !password) {
    showToast('모든 필드를 입력해주세요.', 'error');
    return false;
  }

  if (password !== confirmPassword) {
    showToast('비밀번호가 일치하지 않습니다.', 'error');
    return false;
  }

  if (password.length < 8) {
    showToast('비밀번호는 최소 8자 이상이어야 합니다.', 'error');
    return false;
  }

  const user = {
    email: email,
    name: name,
    loggedIn: true
  };

  storage.set('user', user);
  state.user = user;
  showToast('회원가입 성공! 환영합니다!', 'success');

  setTimeout(() => {
    window.location.href = 'home.html';
  }, 1000);

  return true;
}
```

DB 기반 구현에서는 `storage.set('user', user)`를 제거하고, `POST /api/auth/signup` 응답의 `member`를 `state.user`에만 넣는다. 새로고침 후 로그인 상태는 `GET /api/members/me`와 HttpOnly 쿠키 기반 세션으로 확인한다.

## API 설계

1차 회원가입 API는 하나만 둔다.

```text
POST /api/auth/signup
```

로그인 API가 `POST /api/auth/login`이면 회원가입도 같은 auth namespace에 두는 것이 현재 구조와 가장 맞다. REST 스타일을 더 선호한다면 `POST /api/members`도 가능하지만, 이 프로젝트에서는 프론트 함수가 `app.signup()`이고 로그인 문서도 `/api/auth/login`을 기준으로 하므로 `/api/auth/signup`을 추천한다.

### 요청

```json
{
  "name": "홍길동",
  "email": "test@example.com",
  "password": "password1234",
  "confirmPassword": "password1234",
  "agreeTerms": true
}
```

처리 기준:

```text
name
  trim 후 1자 이상 50자 이하

email
  trim 후 소문자로 정규화
  이메일 형식 검증
  members.email 중복 불가

password
  최소 8자 이상
  confirmPassword와 일치
  DB에는 평문 저장 금지

agreeTerms
  true일 때만 가입 허용
  1차에서는 DB 저장 없이 검증만 해도 됨
```

### 성공 응답

회원가입 성공 후 자동 로그인까지 완료한 응답이다.

```json
{
  "ok": true,
  "message": "회원가입 성공",
  "member": {
    "memberId": 1,
    "email": "test@example.com",
    "name": "홍길동",
    "role": "USER",
    "status": "ACTIVE",
    "level": 1,
    "exp": 0,
    "foodMbti": null,
    "loggedIn": true
  }
}
```

동시에 서버는 세션 쿠키를 내려준다.

```text
Set-Cookie: sessionToken=랜덤토큰; HttpOnly; Path=/; SameSite=Lax
```

프론트는 쿠키 값을 직접 읽지 않는다. 이후 보호 페이지 접근 시 브라우저가 쿠키를 자동으로 보내고, 서버가 `member_sessions`에서 세션을 확인한다.

### 실패 응답

입력값이 잘못된 경우:

```json
{
  "ok": false,
  "message": "입력값을 확인해주세요."
}
```

이메일이 이미 가입된 경우:

```json
{
  "ok": false,
  "message": "이미 사용 중인 이메일입니다."
}
```

권장 HTTP status:

| 상황 | Status |
| --- | --- |
| 가입 성공 | `201 Created` |
| 필수값 누락, 형식 오류, 비밀번호 불일치 | `400 Bad Request` |
| 이메일 중복 | `409 Conflict` |
| 서버/DB 오류 | `500 Internal Server Error` |

## DB 설계

회원 정보는 기존 `docs/DB_schema/login_schema.md`의 `members` 테이블을 사용한다.

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

회원가입 시 직접 넣는 값:

| 컬럼 | 값 |
| --- | --- |
| `email` | 정규화된 이메일 |
| `password_hash` | 해시된 비밀번호 |
| `name` | 사용자 이름 |

DB 기본값에 맡기는 값:

| 컬럼 | 기본값 |
| --- | --- |
| `role` | `USER` |
| `status` | `ACTIVE` |
| `level` | `1` |
| `exp` | `0` |
| `created_at` | 현재 시각 |
| `updated_at` | 현재 시각 |
| `food_mbti` | `NULL` |
| `last_login_at` | `NULL` |

자동 로그인을 하려면 로그인 문서와 같은 세션 테이블도 사용한다.

```text
member_sessions
  member_id
  session_token_hash 또는 refresh_token_hash
  expires_at
  revoked_at
```

토큰 원문은 DB에 저장하지 않고 해시만 저장한다.

## Model 설계

현재 기준 파일:

```text
backend/models/MemberDTO.cpp
```

회원가입에는 다음 데이터 구조가 필요하다.

### SignupRequestDTO

프론트 회원가입 요청을 담는다.

```cpp
class SignupRequestDTO {
public:
  string name;
  string email;
  string password;
  string confirmPassword;
  bool agreeTerms;
};
```

역할:

```text
- Controller가 JSON body에서 만든다.
- Service로 전달된다.
- DB row와 직접 연결되지 않는다.
- password, confirmPassword는 응답으로 절대 내려가지 않는다.
```

### MemberModel

`members` 테이블 row를 담는 내부 모델이다.

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
  optional<string> foodMbti;
  string createdAt;
  string updatedAt;
  optional<string> lastLoginAt;
};
```

`passwordHash`는 내부 검증용으로만 사용한다. 프론트 응답 DTO에는 포함하지 않는다.

### MemberDTO

프론트에 내려줄 회원 정보다.

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
  optional<string> foodMbti;
  string createdAt;
  string updatedAt;
  optional<string> lastLoginAt;
};
```

### SignupResponseDTO

회원가입 결과를 담는다.

```cpp
class SignupResponseDTO {
public:
  bool ok;
  string message;
  optional<MemberDTO> memberDTO;
  optional<string> sessionToken;
};
```

`sessionToken`은 Controller가 `Set-Cookie`를 만들 때만 사용한다. JSON body에는 넣지 않는다.

## Mapper 설계

현재 기준 파일:

```text
backend/mappers/MemberMapper.cpp
```

회원가입에 필요한 Mapper 함수는 다음이다.

```text
findByEmail(email)
insertMember(email, passwordHash, name)
createSession(memberId, sessionTokenHash, expiresAt)
toMemberDTO(memberModel)
```

### findByEmail(email)

이메일 중복 확인에 사용한다.

```text
입력:
  email

출력:
  optional<MemberModel>
```

주의할 점은 `findByEmail()`만으로 중복을 완전히 막을 수 없다는 것이다. 동시에 같은 이메일로 가입 요청이 들어오면 둘 다 "없음"으로 판단할 수 있다. 최종 방어선은 DB의 `UNIQUE KEY uq_members_email (email)`이다.

따라서 Service는 다음 두 가지를 모두 처리해야 한다.

```text
1. insert 전에 findByEmail(email)로 빠르게 중복 확인
2. insert 중 duplicate key 오류가 나면 409 Conflict로 변환
```

### insertMember(email, passwordHash, name)

회원 row를 생성한다.

```sql
INSERT INTO members (
  email,
  password_hash,
  name
) VALUES (?, ?, ?)
```

DB 기본값으로 `role`, `status`, `level`, `exp`가 채워진다.

반환값은 생성된 회원을 다시 조회한 `MemberModel` 또는 생성된 `memberId`를 추천한다. 응답에 기본값까지 포함하려면 insert 후 `findById(memberId)`로 다시 조회하는 방식이 가장 명확하다.

### createSession(memberId, sessionTokenHash, expiresAt)

회원가입 성공 후 자동 로그인할 때 사용한다.

```text
1. Service가 sessionToken 원문 생성
2. Service가 sessionToken 해시 생성
3. Mapper가 해시만 DB에 저장
4. Controller가 원문을 HttpOnly 쿠키로 내려줌
```

### toMemberDTO(memberModel)

DB 내부 모델을 프론트 응답용 DTO로 바꾼다.

```text
MemberModel.passwordHash는 버린다.
MemberDTO에는 memberId, email, name, role, status, level, exp 등을 담는다.
```

## Service 설계

현재 기준 파일:

```text
backend/services/MemberService.cpp
```

회원가입 규칙은 Service에서 판단한다.

필요 함수:

```text
signup(signupRequestDTO)
```

처리 순서:

```text
1. name trim
2. email trim + lowercase 정규화
3. password, confirmPassword 확인
4. agreeTerms 확인
5. name 길이 검증
6. email 형식 검증
7. password 최소 길이 검증
8. password와 confirmPassword 일치 확인
9. memberMapper.findByEmail(email)로 중복 확인
10. password 해시 생성
11. memberMapper.insertMember(email, passwordHash, name)
12. 생성된 회원을 MemberDTO로 변환
13. 자동 로그인용 sessionToken 생성
14. sessionToken 해시를 member_sessions에 저장
15. SignupResponseDTO 반환
```

Service가 하지 말아야 할 일:

```text
- HTTP status 직접 생성
- JSON 직접 생성
- 쿠키 직접 설정
- SQL 직접 작성
```

비밀번호 저장 기준:

```text
개발 중 임시 테스트:
  빠른 연결 확인을 위해 평문 비교를 잠깐 쓸 수는 있음

최종 구현:
  bcrypt, Argon2, PBKDF2 중 하나로 해시
  members.password_hash에는 해시만 저장
```

문서 기준 정답은 항상 해시 저장이다. 평문 비밀번호 저장은 최종 코드에 남기면 안 된다.

## Controller 설계

현재 기준 파일:

```text
backend/controllers/MemberController.cpp
```

Controller는 HTTP 요청/응답만 담당한다.

필요 endpoint:

```text
POST /api/auth/signup
```

Controller가 하는 일:

```text
1. 요청 body가 JSON인지 확인
2. name, email, password, confirmPassword, agreeTerms 읽기
3. SignupRequestDTO 생성
4. MemberService.signup(signupRequestDTO) 호출
5. SignupResponseDTO를 JSON으로 변환
6. 성공이면 sessionToken을 HttpOnly 쿠키로 설정
7. 성공이면 201 Created 반환
8. 실패면 실패 종류에 맞는 HTTP status 반환
```

Controller가 하지 말아야 할 일:

```text
- 이메일 중복 직접 조회
- 비밀번호 해시 생성
- members insert SQL 작성
- MemberModel을 그대로 JSON 반환
```

응답 JSON에서는 `memberDTO`라는 이름 대신 `member`로 내려준다.

```json
{
  "ok": true,
  "message": "회원가입 성공",
  "member": {
    "memberId": 1,
    "email": "test@example.com",
    "name": "홍길동"
  }
}
```

## 프론트 변경 상세

### 1. API 주소 추가

`frontend/html-version/js/app.js` 상단 근처에 API 주소를 둔다.

```js
const API_BASE_URL = 'http://127.0.0.1:8080';
```

이미 로그인 구현에서 추가했다면 그대로 재사용한다.

### 2. 약관 체크박스 id 추가

`frontend/html-version/index.html`의 회원가입 약관 체크박스에 id를 추가한다.

```html
<input type="checkbox" id="signupAgreeTerms" required style="margin-top: 0.25rem;">
```

### 3. signup()을 fetch 기반으로 변경

기존 함수 이름과 인자는 유지하고 내부만 바꾼다.

```js
async function signup(name, email, password, confirmPassword) {
  const agreeTerms = document.getElementById('signupAgreeTerms')?.checked || false;

  if (!name || !email || !password || !confirmPassword) {
    showToast('모든 필드를 입력해주세요.', 'error');
    return false;
  }

  if (!agreeTerms) {
    showToast('이용약관과 개인정보처리방침에 동의해주세요.', 'error');
    return false;
  }

  if (password !== confirmPassword) {
    showToast('비밀번호가 일치하지 않습니다.', 'error');
    return false;
  }

  if (password.length < 8) {
    showToast('비밀번호는 최소 8자 이상이어야 합니다.', 'error');
    return false;
  }

  try {
    const response = await fetch(`${API_BASE_URL}/api/auth/signup`, {
      method: 'POST',
      credentials: 'include',
      headers: {
        'Content-Type': 'application/json'
      },
      body: JSON.stringify({
        name,
        email,
        password,
        confirmPassword,
        agreeTerms
      })
    });

    const data = await response.json();

    if (!response.ok || data.ok === false) {
      throw new Error(data.message || '회원가입에 실패했습니다.');
    }

    state.user = {
      ...data.member,
      loggedIn: true
    };

    showToast('회원가입 성공! 환영합니다!', 'success');

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

중요한 기준:

```text
- localStorage.user에 저장하지 않는다.
- 응답 member는 state.user에만 넣는다.
- fetch에는 credentials: 'include'를 넣는다.
- 회원가입 성공 후 쿠키 기반 세션이 생긴 상태로 home.html로 이동한다.
```

### 4. CSS 변경 여부

`frontend/html-version/css/style.css`에는 이미 회원가입에 필요한 스타일이 있다.

```text
.login-container
.login-card
.tabs
.tab-trigger
.tab-content
.form-input
.btn
.btn-success
.toast
```

1차 구현에서는 CSS 변경이 필요 없다.

추가하면 좋은 정도:

```text
- 요청 중 회원가입 버튼 비활성화
- 중복 요청 방지 loading 상태
- 이메일 중복 오류 시 입력 필드 강조
```

하지만 최소 구현에서는 생략해도 된다.

## 전체 성공 흐름

```text
1. 회원
   - index.html에서 회원가입 탭을 연다.
   - 이름, 이메일, 비밀번호, 비밀번호 확인을 입력한다.
   - 약관에 동의하고 회원가입 버튼을 누른다.

2. View(UI)
   - app.signup(name, email, password, confirmPassword)이 실행된다.
   - 프론트 1차 검증을 한다.
   - POST /api/auth/signup 요청을 보낸다.

3. Controller
   - JSON body를 읽는다.
   - SignupRequestDTO를 만든다.
   - MemberService.signup(signupRequestDTO)를 호출한다.

4. Service
   - 입력값을 검증한다.
   - 이메일을 정규화한다.
   - 중복 이메일을 확인한다.
   - 비밀번호를 해시한다.
   - Mapper에 회원 생성을 요청한다.
   - 자동 로그인 세션을 만든다.

5. Mapper
   - members에 새 회원 row를 insert한다.
   - 생성된 회원을 다시 조회한다.
   - member_sessions에 세션 해시를 저장한다.
   - MemberDTO로 변환한다.

6. DB
   - members row가 생성된다.
   - member_sessions row가 생성된다.

7. Controller
   - SignupResponseDTO를 JSON으로 변환한다.
   - sessionToken을 HttpOnly 쿠키로 내려준다.
   - 201 Created 응답을 반환한다.

8. View(UI)
   - 응답 member를 state.user에 넣는다.
   - home.html로 이동한다.

9. 회원
   - 가입 직후 로그인된 상태로 home.html을 본다.
```

## 실패 흐름

### 필수값 누락

```text
프론트:
  toast로 "모든 필드를 입력해주세요."

백엔드:
  400 Bad Request
```

### 비밀번호 불일치

```text
프론트:
  toast로 "비밀번호가 일치하지 않습니다."

백엔드:
  400 Bad Request
```

### 이메일 중복

```text
Service:
  findByEmail(email) 또는 DB duplicate key 오류로 감지

Controller:
  409 Conflict

View:
  toast로 "이미 사용 중인 이메일입니다."
```

### DB insert 실패

```text
Service:
  실패 응답 반환 또는 예외 전달

Controller:
  500 Internal Server Error

View:
  toast로 "회원가입에 실패했습니다."
```

## 구현 순서

### 1단계: index.html 체크박스 id 추가

약관 체크박스에 `signupAgreeTerms` id를 추가한다.

### 2단계: DTO 추가

`backend/models/MemberDTO.cpp` 또는 분리된 모델 파일에 다음 구조를 준비한다.

```text
SignupRequestDTO
SignupResponseDTO
MemberModel
MemberDTO
```

### 3단계: Mapper 구현

`backend/mappers/MemberMapper.cpp`에 회원가입용 DB 함수를 추가한다.

```text
findByEmail(email)
insertMember(email, passwordHash, name)
findById(memberId)
createSession(memberId, sessionTokenHash, expiresAt)
toMemberDTO(memberModel)
```

### 4단계: Service 구현

`backend/services/MemberService.cpp`에 `signup(signupRequestDTO)`를 추가한다.

Service에서 입력값 검증, 이메일 정규화, 비밀번호 해시, 중복 처리, 세션 생성을 담당한다.

### 5단계: Controller 구현

`backend/controllers/MemberController.cpp`에 `POST /api/auth/signup`을 연결한다.

Controller는 JSON 요청을 DTO로 바꾸고, Service 결과를 HTTP status와 JSON 응답으로 바꾼다.

### 6단계: app.js 변경

`frontend/html-version/js/app.js`의 `signup()`을 `fetch()` 기반으로 바꾼다.

기존 함수 이름은 유지한다.

### 7단계: 보호 페이지 인증 확인

회원가입 성공 후 `home.html`로 이동했을 때, 기존 로그인 문서의 방식대로 `GET /api/members/me`가 성공해야 한다.

즉 회원가입 성공 시 세션 쿠키가 반드시 설정되어 있어야 한다.

## 테스트 계획

브라우저에서 확인할 것:

1. 이름이 비어 있으면 가입되지 않고 toast error가 뜬다.
2. 이메일이 비어 있으면 가입되지 않고 toast error가 뜬다.
3. 비밀번호가 8자 미만이면 가입되지 않는다.
4. 비밀번호와 확인값이 다르면 가입되지 않는다.
5. 약관 체크박스를 체크하지 않으면 가입되지 않는다.
6. 정상 입력이면 `home.html`로 이동한다.
7. `home.html` header에 가입한 이름과 이메일이 표시된다.
8. 새로고침해도 로그인 상태가 유지된다.
9. 같은 이메일로 다시 가입하면 실패한다.

DB에서 확인할 것:

1. `members`에 새 row가 생성된다.
2. `members.email`은 소문자 정규화된 값이다.
3. `members.password_hash`에 평문 비밀번호가 저장되지 않는다.
4. `role`은 `USER`다.
5. `status`는 `ACTIVE`다.
6. `level`은 `1`, `exp`는 `0`이다.
7. 자동 로그인 방식이면 `member_sessions`에 세션 row가 생성된다.

API에서 확인할 것:

1. 성공 시 `201 Created`를 반환한다.
2. 중복 이메일은 `409 Conflict`를 반환한다.
3. 응답 JSON에는 `password`, `confirmPassword`, `passwordHash`, `sessionToken` 원문이 포함되지 않는다.
4. 성공 응답 후 `Set-Cookie`가 내려온다.
5. `GET /api/members/me`가 가입한 회원 정보를 반환한다.

## 나중에 확장할 때

1차 회원가입 후 확장 순서는 다음이 좋다.

```text
1. 이메일 인증 추가
2. 약관 동의 이력 테이블 추가
3. 비밀번호 복잡도 정책 강화
4. 가입 시 IP/userAgent 저장
5. member_login_history에 SIGNUP 이벤트 기록
6. 이메일 중복 확인 버튼 추가
7. 프로필 추가 정보 입력 단계 추가
8. 탈퇴/재가입 정책 추가
```

약관 동의 이력을 DB에 남기려면 다음 중 하나로 확장한다.

```text
간단 방식:
  members.terms_agreed_at
  members.privacy_agreed_at

정교한 방식:
  member_agreements
    agreement_id
    member_id
    agreement_type
    version
    agreed_at
    ip_address
    user_agent
```

1차에서는 화면의 required 체크와 서버의 `agreeTerms = true` 검증만으로 충분하다.

## 최종 정리

회원가입은 현재 화면을 유지하고 `app.signup()` 내부를 실제 API 호출로 바꾸는 방식이 가장 작고 안전하다.

최소 구현 흐름:

```text
회원
  -> View(UI: index.html signup tab, app.signup)
    -> Controller: MemberController.signup()
      -> Service: MemberService.signup()
        -> Mapper: MemberMapper.findByEmail()
          -> DB: members
        -> Mapper: MemberMapper.insertMember()
          -> DB: members
        -> Mapper: MemberMapper.createSession()
          -> DB: member_sessions
        -> Mapper: MemberMapper.toMemberDTO()
      -> Controller: JSON response + Set-Cookie
    -> View(UI): state.user 갱신, home.html 이동
  -> 회원
```

핵심은 세 가지다.

```text
1. 비밀번호는 해시해서 members.password_hash에 저장한다.
2. 중복 이메일은 DB unique key까지 포함해 막는다.
3. 가입 성공 후 자동 로그인 세션을 만들어 기존 html-version UX와 맞춘다.
```
