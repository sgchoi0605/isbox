# ingredients 테이블 스키마

기존 회원 테이블(`members`)에 연결되는 사용자별 냉장고 재료 테이블입니다.  
사용자는 직접 재료를 입력할 수 있고, 공공데이터포털의 `전국통합식품영양성분정보(가공식품)표준데이터` 검색 결과를 선택해 영양성분 값을 함께 저장할 수 있습니다.

## Table

```sql
CREATE TABLE ingredients (
  ingredient_id           BIGINT UNSIGNED NOT NULL AUTO_INCREMENT,              // 재료 고유 ID
  member_id               BIGINT UNSIGNED NOT NULL,                             // 재료를 등록한 회원 ID

  public_food_code        VARCHAR(50) NULL,                                     // 공공데이터 foodCd
  name                    VARCHAR(150) NOT NULL,                                // 재료명
  category                VARCHAR(50) NOT NULL,                                 // 재료 카테고리
  quantity                VARCHAR(50) NOT NULL,                                 // 수량
  unit                    VARCHAR(20) NOT NULL,                                 // 단위
  storage                 ENUM('FRIDGE', 'FREEZER', 'ROOM_TEMP', 'OTHER') NOT NULL, // 보관 위치
  expiry_date             DATE NOT NULL,                                        // 유통기한

  nutrition_basis_amount  VARCHAR(50) NULL,                                     // 영양성분 기준량
  energy_kcal             DECIMAL(10,2) NULL,                                   // 열량(kcal)
  protein_g               DECIMAL(10,2) NULL,                                   // 단백질(g)
  fat_g                   DECIMAL(10,2) NULL,                                   // 지방(g)
  carbohydrate_g          DECIMAL(10,2) NULL,                                   // 탄수화물(g)
  sugar_g                 DECIMAL(10,2) NULL,                                   // 당류(g)
  sodium_mg               DECIMAL(10,2) NULL,                                   // 나트륨(mg)

  source_name             VARCHAR(100) NULL,                                    // 데이터 출처명
  manufacturer_name       VARCHAR(150) NULL,                                    // 제조사명
  import_yn               ENUM('Y', 'N') NULL,                                  // 수입 여부
  origin_country_name     VARCHAR(100) NULL,                                    // 원산지 국가명
  data_base_date          DATE NULL,                                            // 데이터 기준일

  created_at              DATETIME NOT NULL DEFAULT CURRENT_TIMESTAMP,           // 생성 시간
  updated_at              DATETIME NOT NULL DEFAULT CURRENT_TIMESTAMP ON UPDATE CURRENT_TIMESTAMP, // 수정 시간
  deleted_at              DATETIME NULL,                                        // 소프트 삭제 시간

  PRIMARY KEY (ingredient_id),                                                  // 재료 식별 기본키
  KEY idx_ingredients_member_id           (member_id),                          // 회원별 조회용 인덱스
  KEY idx_ingredients_member_storage      (member_id, storage),                 // 회원/보관위치별 조회용 인덱스
  KEY idx_ingredients_member_expiry_date  (member_id, expiry_date),             // 회원/유통기한별 조회용 인덱스
  KEY idx_ingredients_public_food_code    (public_food_code),                   // 공공데이터 코드 조회용 인덱스
  KEY idx_ingredients_member_deleted_at   (member_id, deleted_at),              // 회원/삭제여부 조회용 인덱스

  CONSTRAINT fk_ingredients_member                                             // 회원 테이블 연결 제약
    FOREIGN KEY (member_id)                                                     // 등록 회원 ID를 FK로 사용
    REFERENCES members(member_id)                                               // members.member_id 참조
    ON DELETE CASCADE                                                           // 회원 삭제 시 재료도 함께 삭제
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;
```

## 공공데이터 연동

- endpoint: `https://api.data.go.kr/openapi/tn_pubr_public_nutri_process_info_api`
- 검색 요청 기본값: `serviceKey`, `type=json`, `pageNo`, `numOfRows`, `foodNm`
- 사용자가 음식 단어를 검색하면 API 결과 목록을 보여주고, 선택한 항목의 일부 값을 `ingredients`에 저장합니다.

| 공공데이터 필드 | 저장 컬럼                  |
| ---------------- | -------------------------- |
| `foodCd`         | `public_food_code`         |
| `foodNm`         | `name`                     |
| `nutConSrtrQua`  | `nutrition_basis_amount`   |
| `enerc`          | `energy_kcal`              |
| `prot`           | `protein_g`                |
| `fatce`          | `fat_g`                    |
| `chocdf`         | `carbohydrate_g`           |
| `sugar`          | `sugar_g`                  |
| `nat`            | `sodium_mg`                |
| `srcNm`          | `source_name`              |
| `mfrNm`          | `manufacturer_name`        |
| `imptYn`         | `import_yn`                |
| `cooNm`          | `origin_country_name`      |
| `crtrYmd`        | `data_base_date`           |

## Backend 사용 기준

- 생성 시 `member_id`, `name`, `category`, `quantity`, `unit`, `storage`, `expiry_date`는 필수입니다.
- 공공데이터 검색 결과를 선택하지 않은 재료도 저장할 수 있습니다.
- 공공데이터에서 가져온 값은 추천/표시용 스냅샷이며, 사용자가 직접 입력한 재료 속성을 덮어쓰는 기준값으로 쓰지 않습니다.
- 수정 가능 필드는 `name`, `category`, `quantity`, `unit`, `storage`, `expiry_date`입니다.
- 삭제는 실제 `DELETE` 대신 `deleted_at = NOW()`로 처리합니다.

## CRUD SQL 패턴 (코드 구현 기준)

조회(목록):

```sql
SELECT *
FROM ingredients
WHERE member_id = ? AND deleted_at IS NULL
ORDER BY expiry_date ASC, ingredient_id DESC;
```

조회(단건/권한확인):

```sql
SELECT *
FROM ingredients
WHERE ingredient_id = ? AND member_id = ? AND deleted_at IS NULL;
```

삽입:

```sql
INSERT INTO ingredients (
  member_id, public_food_code, name, category, quantity, unit, storage, expiry_date,
  nutrition_basis_amount, energy_kcal, protein_g, fat_g, carbohydrate_g, sugar_g, sodium_mg,
  source_name, manufacturer_name, import_yn, origin_country_name, data_base_date
) VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?);
```

수정(사용자 수정 가능 필드만):

```sql
UPDATE ingredients
SET
  name = ?,
  category = ?,
  quantity = ?,
  unit = ?,
  storage = ?,
  expiry_date = ?,
  updated_at = NOW()
WHERE ingredient_id = ? AND member_id = ? AND deleted_at IS NULL;
```

삭제(소프트 삭제):

```sql
UPDATE ingredients
SET deleted_at = NOW(), updated_at = NOW()
WHERE ingredient_id = ? AND member_id = ? AND deleted_at IS NULL;
```

## 코드 계층 구성

- `controllers/IngredientController`: HTTP 파라미터/JSON 파싱, 인증 헤더(`X-Member-Id`) 확인, 응답 포맷.
- `services/IngredientService`: 비즈니스 검증(필수값, 길이, storage/date 검증), 공공데이터 API 연동.
- `mappers/IngredientMapper`: 실제 SQL 실행 및 DTO 매핑.

### API 엔드포인트

- `GET /api/ingredients`
- `POST /api/ingredients`
- `PUT /api/ingredients?ingredientId={id}`
- `DELETE /api/ingredients?ingredientId={id}`
- `GET /api/nutrition/processed-foods?keyword={term}`
