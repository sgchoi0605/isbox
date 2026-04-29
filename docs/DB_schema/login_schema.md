# 회원 로그인 DB 스키마 설계

## Summary

현재 프론트엔드는 `localStorage` 기반 임시 로그인 구조이고, 프로젝트 문서상 백엔드는 `Controller -> Service -> Mapper -> DB`, DB는 MySQL을 전제로 합니다.  
회원 인증은 먼저 `members` 중심으로 만들고, 로그인 유지와 보안을 위해 `member_sessions`, `member_login_history`를 함께 둡니다.

## Database Schema

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
기본 키
`member_id BIGINT UNSIGNED NOT NULL AUTO_INCREMENT,`
BIGINT: 매우 큰 정수 (ID용으로 적합)
UNSIGNED: 음수 없음 (0 이상만)
NOT NULL: 반드시 값 필요
AUTO_INCREMENT: 자동 증가

👉 회원 고유 ID (Primary Key 후보)

📧 이메일
`email VARCHAR(255) NOT NULL,`
최대 255자 문자열
반드시 입력

👉 로그인 ID 역할

🔒 비밀번호
`password_hash VARCHAR(255) NOT NULL,`
해시된 비밀번호 저장 (평문 ❌)
👉 보안상 필수 설계
👤 이름
`name VARCHAR(50) NOT NULL,`

👉 사용자 이름

🎭 역할(Role)
`role ENUM('USER', 'ADMIN') NOT NULL DEFAULT 'USER',`
ENUM: 정해진 값만 허용
'USER' 일반 사용자
'ADMIN' 관리자
기본값: USER
🚦 상태(Status)
`status ENUM('ACTIVE', 'WITHDRAWN', 'SUSPENDED') NOT NULL DEFAULT 'ACTIVE',`
ACTIVE: 정상
WITHDRAWN: 탈퇴
SUSPENDED: 정지

👉 계정 상태 관리용

🎮 레벨 시스템
`level INT UNSIGNED NOT NULL DEFAULT 1,`
`exp INT UNSIGNED NOT NULL DEFAULT 0,`
level: 사용자 레벨 (기본 1)
exp: 경험치 (기본 0)

👉 게임/보상 시스템 느낌의 설계

🍔 푸드 MBTI
`food_mbti VARCHAR(20) NULL,`
선택 입력 가능 (NULL)
👉 사용자 취향 데이터
⏱ 생성 시간
`created_at DATETIME NOT NULL DEFAULT CURRENT_TIMESTAMP,`

👉 회원 가입 시각 자동 기록

🔄 수정 시간
`updated_at DATETIME NOT NULL DEFAULT CURRENT_TIMESTAMP ON UPDATE CURRENT_TIMESTAMP,`
DATETIME NOT NULL
날짜 + 시간 저장 (YYYY-MM-DD HH:MM:SS)
반드시 값이 있어야 함

DEFAULT CURRENT_TIMESTAMP

👉 INSERT 시 자동으로 현재 시간 입력
ON UPDATE CURRENT_TIMESTAMP

👉 행이 UPDATE 될 때마다 자동으로 현재 시간으로 갱신

👉 데이터 변경될 때마다 자동 갱신

🕒 마지막 로그인
`last_login_at DATETIME NULL,`

👉 로그인 기록 (없을 수도 있음 → NULL 허용)

🔑 3. 제약 조건 (Constraints)
기본 키
`PRIMARY KEY (member_id),`

👉 각 회원을 유일하게 식별

이메일 유니크
`UNIQUE KEY uq_members_email (email),`

👉 이메일 중복 방지

인덱스
`KEY idx_members_status (status)`

👉 status 검색 성능 향상 (예: ACTIVE 사용자 조회)




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

## Backend Mapping Plan

`models/memberDTO`는 DB의 `members` 테이블과 1차 매핑합니다.

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

optional : 값이 있을 수도 없을 수도 있는 타입

`password_hash`는 로그인 검증용 내부 필드이므로 일반 응답 DTO에는 포함하지 않습니다.  
필요하면 Mapper 내부 전용 `MemberAuthDTO` 또는 `MemberWithPasswordDTO`를 별도로 둡니다.

Mapper는 다음 메서드를 기준으로 설계합니다.

```cpp
findByEmail(email)
findById(memberId)
insertMember(email, passwordHash, name)
updateLastLoginAt(memberId)
updateProfile(memberId, name, email)
updatePassword(memberId, passwordHash)
createSession(memberId, refreshTokenHash, expiresAt, ip, userAgent)
findSessionByRefreshTokenHash(refreshTokenHash)
revokeSession(sessionId)
insertLoginHistory(memberId, email, success, failureReason, ip, userAgent)
```

## Login Flow

회원가입:

1. Controller가 `name`, `email`, `password`, `confirmPassword`를 받음
2. Service가 이메일 중복과 비밀번호 길이 검증
3. Service가 비밀번호를 해시 처리
4. Mapper가 `members`에 저장
5. 응답으로 `memberId`, `email`, `name`, `level`, `exp` 반환

로그인:

1. Controller가 `email`, `password`를 받음
2. Mapper가 `findByEmail(email)`로 회원 조회
3. Service가 `status = ACTIVE`인지 확인
4. Service가 비밀번호 해시 비교
5. 성공 시 `last_login_at` 갱신, 세션 생성, 로그인 기록 저장
6. 실패 시 `member_login_history`에 실패 기록 저장

로그아웃:

1. refresh token 또는 session id 기준으로 세션 조회
2. `member_sessions.revoked_at` 갱신
3. 프론트는 저장된 로그인 상태 제거

## Test Plan

- 회원가입 성공 시 `members`에 회원이 생성되고 이메일은 중복 저장되지 않아야 함
- 같은 이메일로 재가입하면 실패해야 함
- 올바른 이메일/비밀번호 로그인은 성공하고 `last_login_at`이 갱신되어야 함
- 틀린 비밀번호 로그인은 실패하고 `member_login_history`에 실패 기록이 남아야 함
- `WITHDRAWN`, `SUSPENDED` 회원은 로그인할 수 없어야 함
- 로그아웃 시 해당 세션의 `revoked_at`이 채워져야 함
- `MemberDTO` 응답에는 `password_hash`, `refresh_token_hash`가 절대 포함되지 않아야 함

## Assumptions

- DB는 프로젝트 문서 기준으로 MySQL을 사용합니다.
- 비밀번호는 평문 저장 없이 bcrypt, Argon2, PBKDF2 중 하나로 해시합니다.
- 로그인 유지는 access token + refresh token 구조를 권장하되, DB에는 refresh token의 원문이 아니라 해시만 저장합니다.
- 현재 프론트의 `user = { email, name, loggedIn }` 구조는 이후 API 응답의 `member` 객체로 대체합니다.
