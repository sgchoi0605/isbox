회원 로그인 DB 스키마 설계
Summary
현재 프론트엔드는 localStorage 기반 임시 로그인 구조이고, 프로젝트 문서상 백엔드는 Controller -> Service -> Mapper -> DB, DB는 MySQL을 전제로 합니다.
회원 인증은 먼저 members 중심으로 만들고, 로그인 유지와 보안을 위해 member_sessions, member_login_history를 함께 둡니다.

Database Schema
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
Backend Mapping Plan
models/memberDTO는 DB의 members 테이블과 1차 매핑합니다.

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
password_hash는 로그인 검증용 내부 필드이므로 일반 응답 DTO에는 포함하지 않습니다.
필요하면 Mapper 내부 전용 MemberAuthDTO 또는 MemberWithPasswordDTO를 별도로 둡니다.

Mapper는 다음 메서드를 기준으로 설계합니다.

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
Login Flow
회원가입:

Controller가 name, email, password, confirmPassword를 받음
Service가 이메일 중복과 비밀번호 길이 검증
Service가 비밀번호를 해시 처리
Mapper가 members에 저장
응답으로 memberId, email, name, level, exp 반환
로그인:

Controller가 email, password를 받음
Mapper가 findByEmail(email)로 회원 조회
Service가 status = ACTIVE인지 확인
Service가 비밀번호 해시 비교
성공 시 last_login_at 갱신, 세션 생성, 로그인 기록 저장
실패 시 member_login_history에 실패 기록 저장
로그아웃:

refresh token 또는 session id 기준으로 세션 조회
member_sessions.revoked_at 갱신
프론트는 저장된 로그인 상태 제거
Test Plan
회원가입 성공 시 members에 회원이 생성되고 이메일은 중복 저장되지 않아야 함
같은 이메일로 재가입하면 실패해야 함
올바른 이메일/비밀번호 로그인은 성공하고 last_login_at이 갱신되어야 함
틀린 비밀번호 로그인은 실패하고 member_login_history에 실패 기록이 남아야 함
WITHDRAWN, SUSPENDED 회원은 로그인할 수 없어야 함
로그아웃 시 해당 세션의 revoked_at이 채워져야 함
MemberDTO 응답에는 password_hash, refresh_token_hash가 절대 포함되지 않아야 함
Assumptions
DB는 프로젝트 문서 기준으로 MySQL을 사용합니다.
비밀번호는 평문 저장 없이 bcrypt, Argon2, PBKDF2 중 하나로 해시합니다.
로그인 유지는 access token + refresh token 구조를 권장하되, DB에는 refresh token의 원문이 아니라 해시만 저장합니다.
현재 프론트의 user = { email, name, loggedIn } 구조는 이후 API 응답의 member 객체로 대체합니다.