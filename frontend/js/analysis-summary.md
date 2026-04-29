# `frontend/js` 코드 구조 정리 

## 1) 전체 개요

이 `js` 폴더는 **기능별 모듈화된 프론트엔드 구조**입니다.
- 역할 분리 기준
  - `modules/storage.js` : 영속 저장소 래퍼
  - `modules/state.js` : 전역 상태 객체
  - `modules/ui.js` : 공통 UI 제어
  - `modules/auth.js` : 인증 흐름
  - `modules/profile.js` : 사용자 프로필 관리
  - `modules/ingredients.js` : 재료 관리
  - `modules/recipes.js` : 레시피 추천(목 데이터)
  - `modules/community.js` : 커뮤니티 게시글
  - `modules/level.js` : 레벨/경험치
  - `modules/food-mbti.js` : 음식 MBTI
- `app.js`는 이 모듈들을 결합해 앱 진입점 역할(초기화 + 전역 노출)을 합니다.

## 2) 폴더/모듈 구조

- `frontend/js/app.js`
- `frontend/js/modules/storage.js`
- `frontend/js/modules/state.js`
- `frontend/js/modules/ui.js`
- `frontend/js/modules/auth.js`
- `frontend/js/modules/profile.js`
- `frontend/js/modules/ingredients.js`
- `frontend/js/modules/recipes.js`
- `frontend/js/modules/community.js`
- `frontend/js/modules/level.js`
- `frontend/js/modules/food-mbti.js`

## 3) 핵심 의존성/흐름

1. `app.js`가 각 모듈을 import
2. `DOMContentLoaded`에서 페이지 공통 초기화 실행
   - 로그인 필요 페이지에서는 `requireAuth()`로 진입 제어
   - 로그인 정보 표시, 만료 알림 체크, 내비 활성화, 레벨 표시 초기화
3. 공통 이벤트 바인딩
   - 모달 백드롭 클릭 시 닫기
   - 알림 오버레이 클릭 시 패널 닫기
4. `window.app`로 주요 함수/객체 노출
   - HTML 인라인 이벤트(버튼 onclick 등)에서 직접 사용 가능

## 4) 모듈별 정리

### 4.1 `modules/state.js`
전역 상태를 객체로 보관합니다.

- `state`
  - `user`: 로그인 사용자 정보
  - `ingredients`: 재료 목록
  - `recipes`: 추천 레시피 목록
  - `communityPosts`: 커뮤니티 글 목록
  - `expiringItems`: 유통기한 임박 개수
  - `level`, `exp`: 레벨/경험치 상태

### 4.2 `modules/storage.js`
`localStorage` 기반 래퍼입니다.
- `get(key)`: 문자열을 JSON 파싱해 반환, 실패 시 `null`
- `set(key, value)`: JSON 문자열로 직렬화 후 저장
- `remove(key)`: 삭제

### 4.3 `modules/ui.js`
공통 화면 제어 유틸리티.
- `showToast(message, type)`: 알림 토스트 표시
- `openModal/closeModal(modalId)`: 모달 열기/닫기 + body 스크롤 제어
- `toggleNotificationPanel/closeNotificationPanel`: 알림 패널 토글/닫기
- `toggleUserMenu`: 사용자 메뉴 토글
- `switchTab(tabName)`: 탭 UI 활성 상태 전환
- `formatDate(dateString)`: 날짜 포맷
- `activateCurrentNav()`: 현재 경로에 맞춰 네비 active 처리

### 4.4 `modules/auth.js`
로그인/회원 관련 로직(서버 미연동 mock).
- `login(email, password)`: 유효성 검사 후 `state.user` 및 `storage` 저장, 홈 이동
- `signup(name, email, password, confirmPassword)`: 회원가입 검증 후 로그인 상태 생성
- `logout()`: 사용자 제거 후 메인 이동
- `checkAuth()`: 저장소에서 로그인 상태 판별 및 상태 반영
- `requireAuth()`: 실패 시 로그인 페이지로 리다이렉트

### 4.5 `modules/profile.js`
프로필 화면 전용 동작.
- `displayUserInfo()`: DOM 사용자명/이메일 렌더링
- `toggleProfileEdit()`: 편집 모드 on/off
- `cancelProfileEdit()`: 원래 값 복원 + 편집 종료
- `updateProfile()`: 입력 저장 후 `storage/state` 반영
- `changePassword()`: 입력 검증 후 mock 처리(실제 서버 호출 없음)

### 4.6 `modules/ingredients.js`
재료 데이터 관리.
- `loadIngredients()`: 저장소에서 목록 로드
- `saveIngredients(ingredients)`: 저장소/상태 동기화
- `addIngredient(ingredient)`: ID 생성 + 추가 + EXP 보상
- `deleteIngredient(id)`: 삭제
- `getDaysUntilExpiry`, `getExpiryBadgeClass`, `getExpiryText`: 만료일 계산/표시 라벨
- `checkExpiringItems()`: 오늘~3일 이내 임박 개수 계산 및 알림 뱃지 업데이트

### 4.7 `modules/recipes.js`
레시피 추천 도메인(목 데이터 기반).
- `recommendRecipes(selectedIngredientIds)`
  - 현재 재료 중 선택된 것만 추출
- 선택 없으면 토스트 에러 + 빈 배열 반환
- mock 레시피 생성 후 `state.recipes` 저장
- EXP 보상 부여

### 4.8 `modules/community.js`
커뮤니티 게시글 관리.
- `loadCommunityPosts()`: 로컬 저장소에서 로드, 비어 있으면 샘플 seed 데이터 주입
- `saveCommunityPosts(posts)`: 저장 + 상태 반영
- `addCommunityPost(post)`: 새 글 맨 앞 추가 + 작성 시간/상태 부여 + EXP 보상
- `getTimeAgo(dateString)`: 현재 대비 분/시간/일 전 문자열 반환

### 4.9 `modules/level.js`
레벨/경험치 규칙.
- `loadLevelData()`: `userLevel`, `userExp` 로드 후 상태 반영
- `getExpToNextLevel(level)`: 다음 레벨 필요 EXP 계산(기본 `level * 100`)
- `addExp(amount, actionName)`: 더하기, 레벨업 판정/잔여 EXP 처리
- `updateLevelDisplay()`: 레벨/EXP 텍스트와 프로그레스 바 갱신

### 4.10 `modules/food-mbti.js`
음식 MBTI 결과 도메인.
- `mbtiResults`: 유형별 결과 사전(타입, 제목, 설명, 특성, 추천 메뉴)
- `saveFoodMBTI(answers)`
  - 답변 배열을 문자열로 결합해 타입 매핑
- 정확 매칭 실패 시 유사 키 탐색(fallback) 후 기본 타입 사용
- 결과 저장 + EXP 보상
- `loadFoodMBTI()`: 저장된 결과 반환

## 5) `app.js`에서의 노출 포인트

`window.app`에 아래 기능이 바인딩되어 외부(HTML 이벤트, 렌더 함수 등)에서 직접 호출됩니다.
- 인증: `login`, `signup`, `logout`, `checkAuth`, `requireAuth`
- 프로필: `displayUserInfo`, `toggleProfileEdit`, `cancelProfileEdit`, `updateProfile`, `changePassword`
- 재료: `loadIngredients`, `addIngredient`, `deleteIngredient`, `getDaysUntilExpiry`, `getExpiryBadgeClass`, `getExpiryText`, `checkExpiringItems`
- 커뮤니티: `loadCommunityPosts`, `addCommunityPost`, `getTimeAgo`
- 레시피/MBTI: `recommendRecipes`, `saveFoodMBTI`, `loadFoodMBTI`, `mbtiResults`
- UI/레벨: `showToast`, `openModal`, `closeModal`, `toggleNotificationPanel`, `closeNotificationPanel`, `toggleUserMenu`, `switchTab`, `formatDate`, `loadLevelData`, `getExpToNextLevel`, `addExp`, `updateLevelDisplay`
- 상태/저장소 객체: `state`, `storage`

## 6) 특징 정리 (빠른 이해 포인트)

- 저장은 전부 `localStorage` 기반으로 되어 있어 새로고침/페이지 이동 간 정보 유지가 가능합니다.
- 서버 통신 없이 동작하는 **모형(MVP)형 구조**라, 실제 서비스화 시 `auth`, `community`, `recipes`, `profile` 등에서 API 연동 레이어를 바꾸면 됩니다.
- UI와 비즈니스 로직의 분리가 비교적 잘 되어 있어 유지보수/교체 포인트가 명확합니다.
