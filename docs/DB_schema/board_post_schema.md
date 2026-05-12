# board_posts 테이블 스키마

기존 회원 테이블(`members`)에 연결되는 커뮤니티 게시물 테이블입니다.  
사용자는 나눔(`SHARE`) 또는 교환(`EXCHANGE`) 게시물을 작성할 수 있고, 삭제는 실제 row 삭제 대신 상태값을 `DELETED`로 바꾸는 방식으로 처리합니다.

## Table

```sql
CREATE TABLE IF NOT EXISTS board_posts (
  post_id     BIGINT UNSIGNED NOT NULL AUTO_INCREMENT,                         // 게시물 고유 ID
  member_id   BIGINT UNSIGNED NOT NULL,                                        // 작성자 회원 ID

  post_type   ENUM('SHARE', 'EXCHANGE') NOT NULL,                              // 게시물 유형
  title       VARCHAR(100) NOT NULL,                                           // 게시물 제목
  content     TEXT NOT NULL,                                                   // 게시물 본문
  location    VARCHAR(100) NULL,                                               // 나눔/교환 위치
  status      ENUM('AVAILABLE', 'CLOSED', 'DELETED') NOT NULL DEFAULT 'AVAILABLE', // 게시물 상태
  view_count  INT UNSIGNED NOT NULL DEFAULT 0,                                  // 조회수

  created_at  DATETIME NOT NULL DEFAULT CURRENT_TIMESTAMP,                      // 생성 시간
  updated_at  DATETIME NOT NULL DEFAULT CURRENT_TIMESTAMP ON UPDATE CURRENT_TIMESTAMP, // 수정 시간

  PRIMARY KEY (post_id),                                                        // 게시물 식별 기본키
  KEY idx_board_posts_member_id            (member_id),                         // 작성자별 조회용 인덱스
  KEY idx_board_posts_type_status_created  (post_type, status, created_at),     // 유형/상태/최신순 조회용 인덱스
  KEY idx_board_posts_created_at           (created_at),                        // 최신순 조회용 인덱스

  CONSTRAINT fk_board_posts_member                                              // 회원 테이블 연결 제약
    FOREIGN KEY (member_id)                                                     // 작성자 회원 ID를 FK로 사용
    REFERENCES members(member_id)                                               // members.member_id 참조
    ON DELETE CASCADE                                                           // 회원 삭제 시 게시물도 함께 삭제
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;
```

## Backend 사용 기준

- 생성 시 `member_id`, `post_type`, `title`, `content`, `location`, `status`를 저장합니다.
- frontend의 `share`, `exchange` 값은 DB 저장 전에 `SHARE`, `EXCHANGE`로 변환합니다.
- 목록 조회는 `status <> 'DELETED'` 조건으로 삭제 처리된 게시물을 제외합니다.
- 목록 조회는 최신순(`created_at DESC`)으로 최대 50개까지 가져옵니다.
- 삭제는 실제 `DELETE` 대신 `status = 'DELETED'`로 처리합니다.
- 응답에는 `members` 테이블을 조인해 작성자 이름(`author_name`)을 함께 내려줍니다.
