# members 테이블 스키마

현재 backend에서 실제로 사용하는 회원 테이블은 `members` 하나입니다.  
기준 코드는 `backend/mappers/MemberMapper.cpp`와 `backend/mappers/BoardMapper.cpp`이며, 두 곳의 `members` 생성 SQL은 동일합니다.

## Table

```sql
CREATE TABLE IF NOT EXISTS members (
  member_id      BIGINT UNSIGNED NOT NULL AUTO_INCREMENT,                         // 회원 고유 ID
  email          VARCHAR(255) NOT NULL,                                           // 로그인 이메일
  password_hash  VARCHAR(255) NOT NULL,                                           // 비밀번호 해시값
  name           VARCHAR(100) NOT NULL,                                           // 회원 이름
  role           VARCHAR(20) NOT NULL DEFAULT 'USER',                             // 회원 권한
  status         VARCHAR(20) NOT NULL DEFAULT 'ACTIVE',                           // 회원 상태
  level          INT UNSIGNED NOT NULL DEFAULT 1,                                  // 회원 레벨
  exp            INT UNSIGNED NOT NULL DEFAULT 0,                                  // 현재 경험치
  last_login_at  DATETIME NULL,                                                   // 마지막 로그인 시간
  created_at     DATETIME NOT NULL DEFAULT CURRENT_TIMESTAMP,                      // 생성 시간
  updated_at     DATETIME NOT NULL DEFAULT CURRENT_TIMESTAMP ON UPDATE CURRENT_TIMESTAMP, // 수정 시간

  PRIMARY KEY (member_id),                                                        // 회원 식별 기본키
  UNIQUE KEY uq_members_email (email),                                            // 이메일 중복 방지
  KEY idx_members_status (status)                                                 // 상태별 조회용 인덱스
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;
```

## Backend 사용 기준

- 회원가입 시 `email`, `password_hash`, `name`을 저장합니다.
- `email`은 `uq_members_email`로 중복 가입을 막습니다.
- `password_hash`는 API 응답 DTO에는 포함하지 않습니다.
- 로그인 성공 시 `last_login_at`을 `NOW()`로 갱신합니다.
- 경험치 지급 시 `level`, `exp`를 갱신합니다.
- 현재 backend의 로그인 세션은 별도 DB 테이블이 아니라 서버 메모리에서 관리합니다.
