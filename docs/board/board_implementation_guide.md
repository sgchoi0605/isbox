# Board Implementation Guide

## 목표

게시판 기능은 `frontend/html-version/community.html`의 화면 흐름을 기준으로 아주 단순하게 구현한다. 기존 localStorage 기반 게시글 목록/등록 기능을 DB 기반 API로 바꾸고, backend는 반드시 다음 계층을 사용한다.

```text
회원
  -> View(UI)
    -> Controller
      -> Service
        -> Mapper
          -> DB
```

`drogon-db-test2`에서 참고할 핵심은 `src/models/*Model.h`처럼 DB row를 C++ 모델로 만들고, `src/repositories/HealthCheckRepository.h`에서 보여주는 것처럼 Drogon `DbClientPtr`와 `drogon::orm::Mapper<T>` 또는 SQL 실행을 한 계층에 모으는 방식이다. 이 프로젝트에서는 요청한 구조에 맞춰 `Repository` 대신 `backend/mappers/BoardMapper.cpp`가 DB 접근 계층 역할을 맡는다.

## 구현 범위

이번 1차 구현은 다음 기능만 포함한다.

- 게시글 목록 조회
- 게시글 등록
- 게시글 유형 필터: 전체, 나눔, 교환
- 로그인한 회원 기준 작성자 연결
- 게시글 상태: 거래 가능, 완료, 삭제

이번 1차 구현에서 제외한다.

- 댓글
- 이미지 업로드
- 좋아요
- 검색
- 페이지네이션
- 관리자 기능
- 게시글 수정/삭제 UI

## 참고 파일

| 기준 파일 | 참고할 내용 |
| --- | --- |
| `frontend/html-version/community.html` | 게시판 화면 구조, 탭, 게시글 카드, 등록 모달 |
| `frontend/html-version/js/app.js` | localStorage 기반 `loadCommunityPosts`, `addCommunityPost`, `getTimeAgo` 흐름 |
| `frontend/pages/community.html` | 실제 모듈형 frontend에서 수정할 화면 파일 |
| `frontend/js/modules/community.js` | 실제 게시판 데이터 로직을 API 호출로 바꿀 파일 |
| `frontend/js/app.js` | `window.app`에 게시판 함수를 연결하는 진입 파일 |
| `drogon-db-test2/src/models/HealthCheckModel.h` | Drogon ORM Model 작성 방식 |
| `drogon-db-test2/src/models/HealthCheckLogModel.h` | AUTO_INCREMENT PK 모델 작성 방식 |
| `drogon-db-test2/src/repositories/HealthCheckRepository.h` | DB client를 받아 Mapper/SQL을 실행하는 방식 |
| `backend/controllers/MemberController.cpp` | Controller 파일 위치 기준 |
| `backend/services/MemberService.cpp` | Service 파일 위치 기준 |
| `backend/mappers/MemberMapper.cpp` | Mapper 파일 위치 기준 |
| `backend/models/MemberDTO.cpp` | DTO 파일 위치 기준 |

## DB 스키마

`members` 테이블은 기존 로그인 스키마 문서의 `members.member_id`를 사용한다고 가정한다. 게시글은 하나의 테이블만 만든다.

```sql
CREATE TABLE board_posts (
  post_id BIGINT UNSIGNED NOT NULL AUTO_INCREMENT,
  member_id BIGINT UNSIGNED NOT NULL,
  post_type ENUM('SHARE', 'EXCHANGE') NOT NULL,
  title VARCHAR(100) NOT NULL,
  content TEXT NOT NULL,
  location VARCHAR(100) NULL,
  status ENUM('AVAILABLE', 'CLOSED', 'DELETED') NOT NULL DEFAULT 'AVAILABLE',
  view_count INT UNSIGNED NOT NULL DEFAULT 0,
  created_at DATETIME NOT NULL DEFAULT CURRENT_TIMESTAMP,
  updated_at DATETIME NOT NULL DEFAULT CURRENT_TIMESTAMP ON UPDATE CURRENT_TIMESTAMP,

  PRIMARY KEY (post_id),
  KEY idx_board_posts_member_id (member_id),
  KEY idx_board_posts_type_status_created (post_type, status, created_at),
  KEY idx_board_posts_created_at (created_at),

  CONSTRAINT fk_board_posts_member
    FOREIGN KEY (member_id)
    REFERENCES members(member_id)
    ON DELETE CASCADE
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;
```

필드 의미는 다음과 같다.

| 컬럼 | 의미 |
| --- | --- |
| `post_id` | 게시글 PK |
| `member_id` | 작성 회원 ID |
| `post_type` | `SHARE`는 나눔, `EXCHANGE`는 교환 |
| `title` | 게시글 제목 |
| `content` | 게시글 내용. frontend의 기존 `description`을 backend에서는 `content`로 저장한다. |
| `location` | 거래 지역 |
| `status` | `AVAILABLE`, `CLOSED`, `DELETED` |
| `view_count` | 1차 구현에서는 저장만 하고 UI에서는 사용하지 않아도 된다. |
| `created_at` | 생성 시각 |
| `updated_at` | 수정 시각 |

## API 설계

### 게시글 목록

```http
GET /api/board/posts
GET /api/board/posts?type=share
GET /api/board/posts?type=exchange
```

응답 예시:

```json
{
  "ok": true,
  "posts": [
    {
      "postId": 1,
      "memberId": 3,
      "type": "share",
      "title": "치킨 나눔합니다",
      "content": "남은 치킨을 나눔합니다.",
      "location": "서울 강남구",
      "authorName": "김철수",
      "status": "available",
      "createdAt": "2026-05-11 12:30:00"
    }
  ]
}
```

### 게시글 등록

```http
POST /api/board/posts
Content-Type: application/json
```

요청 예시:

```json
{
  "type": "share",
  "title": "치킨 나눔합니다",
  "content": "남은 치킨을 나눔합니다.",
  "location": "서울 강남구"
}
```

응답 예시:

```json
{
  "ok": true,
  "post": {
    "postId": 1,
    "memberId": 3,
    "type": "share",
    "title": "치킨 나눔합니다",
    "content": "남은 치킨을 나눔합니다.",
    "location": "서울 강남구",
    "authorName": "김철수",
    "status": "available",
    "createdAt": "2026-05-11 12:30:00"
  }
}
```

회원 인증이 아직 완성되지 않았다면 개발 단계에서만 `X-Member-Id: 1` 헤더를 임시로 허용한다. 최종 구조에서는 Controller가 세션 또는 토큰에서 `memberId`를 꺼내야 하며, frontend가 작성자 이름을 직접 보내면 안 된다.

## Backend 변경 가이드

### 1. `backend/models/BoardPostModel.h` 추가

역할: `board_posts` DB row를 Drogon ORM Mapper가 다룰 수 있는 C++ 모델로 표현한다.

`drogon-db-test2/src/models/HealthCheckLogModel.h`를 기준으로 만든다. `post_id`가 AUTO_INCREMENT이므로 `updateId(std::uint64_t id)`에서 insert 후 발급된 ID를 모델에 반영한다.

필수 기능:

- `tableName = "board_posts"`
- `primaryKeyName = "post_id"`
- `sqlForFindingByPrimaryKey()`
- `sqlForInserting(bool &needSelection)`
- `outputArgs(SqlBinder &binder)`
- `updateColumns()`
- `updateArgs(SqlBinder &binder)`
- `getPrimaryKey()`
- `updateId(std::uint64_t id)`
- getter: `postId()`, `memberId()`, `type()`, `title()`, `content()`, `location()`, `status()`, `createdAt()`

최소 모델 필드:

```cpp
std::uint64_t postId_;
std::uint64_t memberId_;
std::string postType_;
std::string title_;
std::string content_;
std::optional<std::string> location_;
std::string status_;
std::string createdAt_;
```

주의:

- DB 컬럼명은 `post_type`, C++ getter나 JSON 응답에서는 `type`으로 노출한다.
- DB에는 대문자 enum `SHARE`, `EXCHANGE`를 저장하고, frontend 응답에는 소문자 `share`, `exchange`로 변환한다.

### 2. `backend/models/BoardPostDTO.cpp` 추가

역할: Controller와 Service가 주고받는 요청/응답 데이터를 정의한다.

필수 DTO:

```cpp
class CreateBoardPostRequest {
public:
  std::string type;
  std::string title;
  std::string content;
  std::optional<std::string> location;
};

class BoardPostDTO {
public:
  unsigned long long postId;
  unsigned long long memberId;
  std::string type;
  std::string title;
  std::string content;
  std::optional<std::string> location;
  std::string authorName;
  std::string status;
  std::string createdAt;
};
```

현재 `MemberDTO.cpp`처럼 `.cpp`에 class를 바로 두는 방식이 쓰이고 있지만, Drogon ORM Model은 여러 파일에서 include되어야 하므로 실제 구현 때는 `.h` 분리를 권장한다. 최소 구현에서는 기존 프로젝트 컨벤션에 맞춰 `BoardPostDTO.cpp`로 시작해도 된다.

### 3. `backend/mappers/BoardMapper.cpp` 추가

역할: DB 접근을 담당한다. Service는 SQL을 몰라야 한다.

필수 함수:

```cpp
class BoardMapper {
public:
  explicit BoardMapper(drogon::orm::DbClientPtr dbClient);

  std::vector<BoardPostDTO> findPosts(std::optional<std::string> type);
  BoardPostDTO insertPost(unsigned long long memberId, const CreateBoardPostRequest &request);
  std::optional<BoardPostDTO> findById(unsigned long long postId);

private:
  void ensureTableForLocalDev();
};
```

구현 기준:

- `insertPost()`는 `drogon::orm::Mapper<BoardPostModel>`을 사용한다.
- `findPosts()`는 작성자 이름이 필요하므로 `board_posts`와 `members`를 join하는 SQL을 사용한다.
- `status = 'DELETED'`인 글은 목록에서 제외한다.
- 1차 구현은 `ORDER BY p.created_at DESC LIMIT 50`만 사용한다.

목록 조회 SQL 예시:

```sql
SELECT
  p.post_id,
  p.member_id,
  p.post_type,
  p.title,
  p.content,
  p.location,
  p.status,
  p.created_at,
  m.name AS author_name
FROM board_posts p
JOIN members m ON m.member_id = p.member_id
WHERE p.status <> 'DELETED'
  AND (? IS NULL OR p.post_type = ?)
ORDER BY p.created_at DESC
LIMIT 50;
```

`ensureTableForLocalDev()`는 `drogon-db-test2`의 `ensureTables()`처럼 로컬 개발 편의를 위한 임시 DDL로만 둔다. 운영 기준으로는 `database/migrations`나 별도 SQL 문서로 관리해야 한다.

### 4. `backend/services/BoardService.cpp` 추가

역할: 게시판 use case와 검증을 담당한다.

필수 함수:

```cpp
class BoardService {
public:
  explicit BoardService(BoardMapper mapper);

  std::vector<BoardPostDTO> getPosts(std::optional<std::string> type);
  BoardPostDTO createPost(unsigned long long memberId, const CreateBoardPostRequest &request);
};
```

Service에서 처리할 규칙:

- `type`은 `share`, `exchange`, `all`만 허용한다.
- DB 저장 전 `share -> SHARE`, `exchange -> EXCHANGE`로 정규화한다.
- 제목은 빈 값이면 실패한다.
- 제목은 100자 이하로 제한한다.
- 내용은 빈 값이면 실패한다.
- 위치는 선택값이다.
- memberId가 없으면 실패한다.

Service에서 하지 말아야 할 일:

- HTTP response 생성
- SQL 작성
- Drogon request 객체 직접 접근
- frontend 전용 DOM/문구 처리

### 5. `backend/controllers/BoardController.cpp` 추가

역할: HTTP 요청을 받고 Service를 호출한 뒤 JSON 응답을 반환한다.

필수 endpoint:

```text
GET  /api/board/posts
POST /api/board/posts
```

Controller에서 처리할 일:

- query string `type` 읽기
- JSON body 읽기
- 세션 또는 토큰에서 `memberId` 읽기
- 개발 중이면 임시로 `X-Member-Id` 헤더 읽기
- `BoardService` 호출
- 성공/실패 JSON 응답 만들기

Controller에서 하지 말아야 할 일:

- SQL 작성
- DB client 직접 사용
- 게시글 제목/내용 비즈니스 검증을 전부 처리
- author 이름을 request body에서 신뢰

### 6. `backend/common.hpp` 보강

현재 `common.hpp`는 최소 include만 있다. Board 기능 구현 시 다음 타입을 쓰게 되므로 공통 include를 추가한다.

```cpp
#include <cstdint>
#include <optional>
#include <string>
#include <vector>
#include <json/json.h>
```

단, 모든 파일에 무조건 공통 include를 몰아넣기보다 실제 사용하는 파일에서 직접 include하는 편이 더 명확하다.

### 7. 서버 진입점에 route 연결

현재 backend 폴더에는 서버 진입점이 보이지 않고, `drogon-db-test2/src/main.cpp`가 학습용 예시 역할을 한다. 실제 backend 서버 파일이 생기면 `main.cpp`는 다음 정도만 남기는 것을 목표로 한다.

```cpp
int main()
{
    configureMariaDbRuntimeForWindowsIfNeeded();
    drogon::app().loadConfigFile("config.json");
    registerBoardRoutes();
    drogon::app().run();
}
```

`drogon-db-test2/src/main.cpp`처럼 `registerHandler()` lambda 안에 DB 로직을 직접 넣지 말고, lambda 또는 Controller에서는 `BoardService`만 호출한다.

## Frontend 변경 가이드

### 1. `frontend/pages/community.html` 수정

현재 기능:

- `renderPosts()`가 `app.loadCommunityPosts()`를 동기 호출한다.
- `handleAddPost()`가 `app.addCommunityPost(post)`를 호출한다.
- 게시글 등록 모달에 `author` 입력칸이 있다.
- 기존 데이터 필드는 `description`을 사용한다.

변경할 내용:

- `renderPosts()`를 `async function renderPosts()`로 바꾼다.
- `const posts = await app.loadCommunityPosts(currentFilter);`로 API 목록을 가져온다.
- `handleAddPost()`를 async로 바꾸고 `await app.addCommunityPost(post)`를 호출한다.
- 등록 후 `await renderPosts()`를 호출한다.
- `author` 입력칸은 제거한다. 작성자는 로그인 회원에서 결정한다.
- `description` 입력 id와 변수명을 `content`로 바꾼다. 화면에 보여줄 때도 `post.content`를 사용한다.
- type 필터는 기존 `all`, `share`, `exchange`를 그대로 유지한다.

기존:

```js
const posts = app.loadCommunityPosts();
```

변경:

```js
const posts = await app.loadCommunityPosts(currentFilter);
```

기존:

```js
const post = {
  type: document.getElementById('type').value,
  title: document.getElementById('title').value,
  description: document.getElementById('description').value,
  location: document.getElementById('location').value,
  author: document.getElementById('author').value
};
```

변경:

```js
const post = {
  type: document.getElementById('type').value,
  title: document.getElementById('title').value,
  content: document.getElementById('content').value,
  location: document.getElementById('location').value
};
```

### 2. `frontend/js/modules/community.js` 수정

현재 기능:

- localStorage에서 `communityPosts`를 읽고 쓴다.
- 샘플 게시글을 localStorage에 넣는다.
- 등록 시 localStorage 배열 앞에 추가한다.

변경할 내용:

- localStorage 저장 로직을 제거한다.
- API 호출 기반으로 바꾼다.
- `state.communityPosts`에는 API 응답 결과만 캐싱한다.
- `addExp(30, ...)`는 backend 저장 성공 후 호출한다. 나중에 경험치도 backend에서 처리하게 되면 제거한다.

기준 코드:

```js
import { state } from './state.js';
import { addExp } from './level.js';

const API_BASE = '/api/board/posts';

export async function loadCommunityPosts(filter = 'all') {
  const query = filter && filter !== 'all' ? `?type=${encodeURIComponent(filter)}` : '';
  const response = await fetch(`${API_BASE}${query}`, {
    headers: buildAuthHeaders()
  });

  if (!response.ok) {
    throw new Error('게시글 목록을 불러오지 못했습니다.');
  }

  const body = await response.json();
  state.communityPosts = body.posts || [];
  return state.communityPosts;
}

export async function addCommunityPost(post) {
  const response = await fetch(API_BASE, {
    method: 'POST',
    headers: {
      'Content-Type': 'application/json',
      ...buildAuthHeaders()
    },
    body: JSON.stringify(post)
  });

  if (!response.ok) {
    throw new Error('게시글을 등록하지 못했습니다.');
  }

  const body = await response.json();
  addExp(30, '커뮤니티 게시글 작성');
  return body.post;
}

function buildAuthHeaders() {
  const user = state.user;
  if (!user || !user.memberId) return {};

  // 인증 구현 전 개발용 임시 헤더다. 최종 구현에서는 Authorization 토큰 또는 세션 쿠키로 대체한다.
  return { 'X-Member-Id': String(user.memberId) };
}
```

### 3. `frontend/js/modules/auth.js` 수정

현재 localStorage user:

```js
{
  email,
  name,
  loggedIn: true
}
```

게시판 API를 회원 기준으로 호출하려면 최소한 다음 형태가 필요하다.

```js
{
  memberId: 1,
  email,
  name,
  loggedIn: true
}
```

로그인 API가 아직 없다면 개발용으로만 `memberId: 1`을 넣는다. 로그인 API가 생기면 backend 응답의 `member.memberId`를 저장한다.

### 4. `frontend/js/app.js` 확인

현재 이미 다음 함수를 window에 연결하고 있다.

```js
loadCommunityPosts,
addCommunityPost,
getTimeAgo
```

`community.js`의 함수가 async로 바뀌어도 export 이름을 유지하면 `app.js`는 큰 변경이 필요 없다. 단, `community.html`에서 호출하는 쪽은 반드시 `await`를 붙여야 한다.

### 5. `frontend/html-version/community.html`의 역할

`frontend/html-version`은 구현 기준 화면을 참고하는 용도로만 둔다. 실제 수정 대상은 `frontend/pages/community.html`과 `frontend/js/modules/community.js`다.

html-version에서 가져올 UI 요소:

- 상단 커뮤니티 제목 영역
- `all/share/exchange` 탭
- 게시글 카드 레이아웃
- 글쓰기 모달

html-version에서 그대로 가져오지 말아야 할 것:

- localStorage 기반 데이터 처리
- `author` 직접 입력
- `description` 필드명
- 샘플 게시글 자동 삽입

## 데이터 흐름

### 목록 조회

```text
회원이 커뮤니티 화면 진입
  -> frontend/pages/community.html DOMContentLoaded
    -> renderPosts()
      -> app.loadCommunityPosts(currentFilter)
        -> frontend/js/modules/community.js fetch GET /api/board/posts
          -> BoardController.listPosts()
            -> BoardService.getPosts(type)
              -> BoardMapper.findPosts(type)
                -> DB SELECT board_posts JOIN members
              <- BoardPostDTO list
            <- JSON posts
          <- posts
      -> 게시글 카드 렌더링
```

### 게시글 등록

```text
회원이 글쓰기 모달 작성
  -> handleAddPost(event)
    -> app.addCommunityPost({ type, title, content, location })
      -> frontend/js/modules/community.js fetch POST /api/board/posts
        -> BoardController.createPost()
          -> memberId 추출
          -> request JSON 파싱
          -> BoardService.createPost(memberId, request)
            -> 입력값 검증
            -> type 정규화
            -> BoardMapper.insertPost(memberId, request)
              -> DB INSERT board_posts
              -> DB SELECT created post with author
            <- BoardPostDTO
          <- JSON post
      -> 모달 닫기
      -> renderPosts()
```

## 구현 순서

1. DB에 `board_posts` 테이블을 만든다.
2. `backend/models/BoardPostModel.h`를 만든다.
3. `backend/models/BoardPostDTO.cpp`를 만든다.
4. `backend/mappers/BoardMapper.cpp`를 만든다.
5. `backend/services/BoardService.cpp`를 만든다.
6. `backend/controllers/BoardController.cpp`를 만든다.
7. Drogon 서버 진입점에서 `/api/board/posts` route를 연결한다.
8. `frontend/js/modules/auth.js`의 user에 `memberId`를 포함한다.
9. `frontend/js/modules/community.js`를 localStorage에서 fetch API로 바꾼다.
10. `frontend/pages/community.html`의 inline script를 async API 흐름에 맞춘다.
11. 브라우저에서 커뮤니티 화면 진입, 글 등록, 필터 조회를 확인한다.

## 최소 테스트 기준

- 로그인한 회원 정보에 `memberId`가 있어야 게시글 등록이 된다.
- `GET /api/board/posts`는 삭제되지 않은 게시글을 최신순으로 반환한다.
- `GET /api/board/posts?type=share`는 나눔 글만 반환한다.
- `GET /api/board/posts?type=exchange`는 교환 글만 반환한다.
- `POST /api/board/posts`는 `title`, `content`, `type`이 없으면 실패한다.
- frontend 등록 모달에는 작성자 입력칸이 없어야 한다.
- 새 글 등록 후 목록 상단에 새 글이 보여야 한다.
- DB에는 `member_id`가 저장되어야 하고, 응답에는 회원 이름이 `authorName`으로 내려와야 한다.

## 최종 파일별 변경 요약

| 파일 | 작업 | 기능 |
| --- | --- | --- |
| `backend/models/BoardPostModel.h` | 추가 | `board_posts` ORM row model |
| `backend/models/BoardPostDTO.cpp` | 추가 | 게시글 요청/응답 DTO |
| `backend/mappers/BoardMapper.cpp` | 추가 | 게시글 SELECT/INSERT DB 접근 |
| `backend/services/BoardService.cpp` | 추가 | 게시글 검증, type 정규화, use case 처리 |
| `backend/controllers/BoardController.cpp` | 추가 | `/api/board/posts` GET/POST 처리 |
| `backend/common.hpp` | 필요 시 수정 | 공통 타입 include 보강 |
| `frontend/pages/community.html` | 수정 | localStorage 동기 렌더링을 async API 렌더링으로 변경 |
| `frontend/js/modules/community.js` | 수정 | localStorage 제거, fetch API 호출로 변경 |
| `frontend/js/modules/auth.js` | 수정 | 로그인 user에 `memberId` 저장 |
| `frontend/js/app.js` | 확인 | async 게시판 함수 export 이름 유지 |
| `frontend/html-version/community.html` | 참고만 | UI 구조 기준 파일, 직접 구현 대상 아님 |

