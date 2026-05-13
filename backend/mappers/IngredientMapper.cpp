// IngredientMapper의 인터페이스 선언을 포함한다.
#include "IngredientMapper.h"

// drogon 앱/ORM 접근(앱 컨텍스트, DB 클라이언트, Row 변환)에 필요하다.
#include <drogon/drogon.h>

// DDL 1회 실행 보장을 위한 원자 플래그.
#include <atomic>
// optional<double>를 SQL 바인딩 문자열(소수점 고정)로 변환할 때 사용한다.
#include <iomanip>
// 숫자를 문자열로 포맷팅하기 위한 스트림 유틸리티.
#include <sstream>

// 파일 내부 전용 헬퍼 함수 모음(외부 심볼 노출 방지).
namespace
{

// 문자열 컬럼을 optional<string>으로 안전하게 읽는다.
// NULL 또는 빈 문자열은 값 없음(nullopt)으로 통일한다.
std::optional<std::string> optionalStringField(const drogon::orm::Row &row,
                                               const char *column)
{
    // 컬럼 접근 시 RowProxy를 받아 null 여부를 먼저 검사한다.
    const auto &field = row[column];
    if (field.isNull())
    {
        return std::nullopt;
    }

    // DB 값이 공백 문자열일 수 있으므로 문자열로 꺼낸 뒤 빈 값도 nullopt 처리한다.
    const auto value = field.as<std::string>();
    if (value.empty())
    {
        return std::nullopt;
    }
    return value;
}

// 숫자 컬럼을 optional<double>로 안전하게 읽는다.
// NULL이면 값 없음(nullopt)으로 처리한다.
std::optional<double> optionalDoubleField(const drogon::orm::Row &row,
                                          const char *column)
{
    // 숫자 컬럼도 동일하게 null 안전성을 먼저 확보한다.
    const auto &field = row[column];
    if (field.isNull())
    {
        return std::nullopt;
    }
    // ORM이 내부 타입을 double로 변환해 준 값을 그대로 반환한다.
    return field.as<double>();
}

// optional<string>을 INSERT 바인딩용 문자열로 변환한다.
// 값이 없으면 빈 문자열을 전달하고, SQL의 NULLIF(?, '')로 NULL 처리한다.
std::string optionalStringToInsertValue(const std::optional<std::string> &value)
{
    // 이 프로젝트의 INSERT 정책: optional 없음 -> "" 전달 -> SQL NULLIF로 NULL.
    if (!value.has_value())
    {
        return "";
    }
    // 값이 있으면 원문 문자열을 그대로 바인딩한다.
    return *value;
}

// optional<double>을 INSERT 바인딩용 문자열로 변환한다.
// 값이 없으면 빈 문자열을 전달하고, SQL의 NULLIF(?, '')로 NULL 처리한다.
std::string optionalDoubleToInsertValue(const std::optional<double> &value)
{
    // optional 없음은 string과 동일하게 ""로 치환한다.
    if (!value.has_value())
    {
        return "";
    }

    // DECIMAL(10,2) 컬럼에 맞춰 소수점 2자리로 문자열화한다.
    std::ostringstream out;
    out << std::fixed << std::setprecision(2) << *value;
    return out.str();
}

}  // namespace

namespace ingredient
{

std::vector<IngredientDTO> IngredientMapper::findIngredientsByMemberId(
    std::uint64_t memberId) const
{
    // 로컬 개발 환경에서 스키마가 없는 경우를 대비해 테이블을 보장한다.
    ensureTablesForLocalDev();

    // 회원 소유 + 미삭제 조건으로 목록 조회한다.
    // 날짜/시각은 SQL에서 미리 문자열 포맷팅하여 DTO 매핑 시 직렬화 비용을 줄인다.
    const auto dbClient = drogon::app().getDbClient("default");
    const auto result = dbClient->execSqlSync(
        "SELECT "
        "ingredient_id, member_id, public_food_code, name, category, quantity, unit, storage, "
        "DATE_FORMAT(expiry_date, '%Y-%m-%d') AS expiry_date, "
        "nutrition_basis_amount, energy_kcal, protein_g, fat_g, carbohydrate_g, sugar_g, sodium_mg, "
        "source_name, manufacturer_name, import_yn, origin_country_name, "
        "DATE_FORMAT(data_base_date, '%Y-%m-%d') AS data_base_date, "
        "COALESCE(DATE_FORMAT(created_at, '%Y-%m-%d %H:%i:%s'), '') AS created_at, "
        "COALESCE(DATE_FORMAT(updated_at, '%Y-%m-%d %H:%i:%s'), '') AS updated_at "
        "FROM ingredients "
        "WHERE member_id = ? AND deleted_at IS NULL "
        "ORDER BY storage ASC, expiry_date ASC, ingredient_id DESC",
        memberId);

    // DB 결과를 DTO 벡터로 변환한다.
    // reserve로 재할당을 줄여 목록 조회 성능을 안정화한다.
    std::vector<IngredientDTO> ingredients;
    ingredients.reserve(result.size());
    for (const auto &row : result)
    {
        // 단일 row 매핑 로직은 rowToIngredientDTO로 일원화한다.
        ingredients.push_back(rowToIngredientDTO(row));
    }
    return ingredients;
}

std::optional<IngredientDTO> IngredientMapper::findByIdForMember(
    std::uint64_t ingredientId,
    std::uint64_t memberId) const
{
    // 로컬 개발 환경에서 스키마가 없는 경우를 대비해 테이블을 보장한다.
    ensureTablesForLocalDev();

    // 재료 1건 조회: 재료 ID + 회원 ID + 미삭제 조건을 동시에 만족해야 한다.
    // 즉, 타 회원 데이터 접근과 이미 삭제된 데이터 접근을 모두 차단한다.
    const auto dbClient = drogon::app().getDbClient("default");
    const auto result = dbClient->execSqlSync(
        "SELECT "
        "ingredient_id, member_id, public_food_code, name, category, quantity, unit, storage, "
        "DATE_FORMAT(expiry_date, '%Y-%m-%d') AS expiry_date, "
        "nutrition_basis_amount, energy_kcal, protein_g, fat_g, carbohydrate_g, sugar_g, sodium_mg, "
        "source_name, manufacturer_name, import_yn, origin_country_name, "
        "DATE_FORMAT(data_base_date, '%Y-%m-%d') AS data_base_date, "
        "COALESCE(DATE_FORMAT(created_at, '%Y-%m-%d %H:%i:%s'), '') AS created_at, "
        "COALESCE(DATE_FORMAT(updated_at, '%Y-%m-%d %H:%i:%s'), '') AS updated_at "
        "FROM ingredients "
        "WHERE ingredient_id = ? AND member_id = ? AND deleted_at IS NULL "
        "LIMIT 1",
        ingredientId,
        memberId);

    // 조회 결과가 없으면 nullopt를 반환해 호출 측에서 404/권한 실패를 분기한다.
    if (result.empty())
    {
        return std::nullopt;
    }

    // LIMIT 1 조회이므로 첫 row만 DTO로 변환해 반환한다.
    return rowToIngredientDTO(result[0]);
}

std::optional<IngredientDTO> IngredientMapper::insertIngredient(
    std::uint64_t memberId,
    const CreateIngredientRequestDTO &request) const
{
    // 로컬 개발 환경에서 스키마가 없는 경우를 대비해 테이블을 보장한다.
    ensureTablesForLocalDev();

    // 신규 재료를 삽입한다.
    // optional 필드는 빈 문자열로 바인딩하고 SQL의 NULLIF(?, '')로 NULL 변환한다.
    // 문자열 바인딩을 통일하면 C++ 측 NULL 분기 코드를 단순화할 수 있다.
    const auto dbClient = drogon::app().getDbClient("default");
    const auto insertResult = dbClient->execSqlSync(
        "INSERT INTO ingredients ("
        "member_id, public_food_code, name, category, quantity, unit, storage, expiry_date, "
        "nutrition_basis_amount, energy_kcal, protein_g, fat_g, carbohydrate_g, sugar_g, sodium_mg, "
        "source_name, manufacturer_name, import_yn, origin_country_name, data_base_date"
        ") VALUES ("
        "?, NULLIF(?, ''), ?, ?, ?, ?, ?, ?, "
        "NULLIF(?, ''), NULLIF(?, ''), NULLIF(?, ''), NULLIF(?, ''), NULLIF(?, ''), NULLIF(?, ''), NULLIF(?, ''), "
        "NULLIF(?, ''), NULLIF(?, ''), NULLIF(?, ''), NULLIF(?, ''), NULLIF(?, '')"
        ")",
        memberId,
        optionalStringToInsertValue(request.publicFoodCode),
        request.name,
        request.category,
        request.quantity,
        request.unit,
        request.storage,
        request.expiryDate,
        optionalStringToInsertValue(request.nutritionBasisAmount),
        optionalDoubleToInsertValue(request.energyKcal),
        optionalDoubleToInsertValue(request.proteinG),
        optionalDoubleToInsertValue(request.fatG),
        optionalDoubleToInsertValue(request.carbohydrateG),
        optionalDoubleToInsertValue(request.sugarG),
        optionalDoubleToInsertValue(request.sodiumMg),
        optionalStringToInsertValue(request.sourceName),
        optionalStringToInsertValue(request.manufacturerName),
        optionalStringToInsertValue(request.importYn),
        optionalStringToInsertValue(request.originCountryName),
        optionalStringToInsertValue(request.dataBaseDate));

    // auto increment ID를 받아 방금 삽입한 재료를 재조회한다.
    // 재조회는 DB 기본값(created_at/updated_at 등)을 포함한 최종 상태를 반환하기 위함이다.
    const auto insertedId = insertResult.insertId();
    if (insertedId == 0ULL)
    {
        // insertId가 0이면 삽입 식별에 실패한 비정상 케이스로 간주한다.
        return std::nullopt;
    }

    return findByIdForMember(insertedId, memberId);
}

bool IngredientMapper::updateEditableFields(
    std::uint64_t ingredientId,
    std::uint64_t memberId,
    const UpdateIngredientRequestDTO &request) const
{
    // 로컬 개발 환경에서 스키마가 없는 경우를 대비해 테이블을 보장한다.
    ensureTablesForLocalDev();

    // 수정 허용 필드(name/category/quantity/unit/storage/expiry_date)만 갱신한다.
    // 소유자(member_id)와 미삭제 조건을 만족하는 row만 업데이트된다.
    const auto dbClient = drogon::app().getDbClient("default");
    const auto result = dbClient->execSqlSync(
        "UPDATE ingredients "
        "SET name = ?, category = ?, quantity = ?, unit = ?, storage = ?, expiry_date = ? "
        "WHERE ingredient_id = ? AND member_id = ? AND deleted_at IS NULL",
        request.name,
        request.category,
        request.quantity,
        request.unit,
        request.storage,
        request.expiryDate,
        ingredientId,
        memberId);

    // 실제 변경된 row가 있을 때만 true를 반환한다.
    return result.affectedRows() > 0;
}

bool IngredientMapper::markDeleted(std::uint64_t ingredientId,
                                   std::uint64_t memberId) const
{
    // 로컬 개발 환경에서 스키마가 없는 경우를 대비해 테이블을 보장한다.
    ensureTablesForLocalDev();

    // 물리 삭제 대신 deleted_at을 현재 시각으로 채워 소프트 삭제한다.
    // 이후 모든 조회 쿼리에서 deleted_at IS NULL 조건으로 숨긴다.
    const auto dbClient = drogon::app().getDbClient("default");
    const auto result = dbClient->execSqlSync(
        "UPDATE ingredients "
        "SET deleted_at = NOW() "
        "WHERE ingredient_id = ? AND member_id = ? AND deleted_at IS NULL",
        ingredientId,
        memberId);

    // 실제 변경된 row가 있을 때만 true를 반환한다.
    return result.affectedRows() > 0;
}

IngredientDTO IngredientMapper::rowToIngredientDTO(const drogon::orm::Row &row)
{
    // DB row를 API 응답 구조에 맞춘 DTO로 매핑한다.
    // 필수 컬럼은 직접 변환, nullable 컬럼은 optional 헬퍼를 통해 일관 처리한다.
    IngredientDTO dto;

    // 기본 식별/분류 정보
    dto.ingredientId = row["ingredient_id"].as<std::uint64_t>();
    dto.memberId = row["member_id"].as<std::uint64_t>();
    dto.publicFoodCode = optionalStringField(row, "public_food_code");
    dto.name = row["name"].as<std::string>();
    dto.category = row["category"].as<std::string>();
    dto.quantity = row["quantity"].as<std::string>();
    dto.unit = row["unit"].as<std::string>();
    dto.storage = row["storage"].as<std::string>();
    // 조회 SQL에서 DATE_FORMAT을 적용했으므로 YYYY-MM-DD 문자열로 들어온다.
    dto.expiryDate = row["expiry_date"].as<std::string>();

    // 영양 정보(optional)
    dto.nutritionBasisAmount = optionalStringField(row, "nutrition_basis_amount");
    dto.energyKcal = optionalDoubleField(row, "energy_kcal");
    dto.proteinG = optionalDoubleField(row, "protein_g");
    dto.fatG = optionalDoubleField(row, "fat_g");
    dto.carbohydrateG = optionalDoubleField(row, "carbohydrate_g");
    dto.sugarG = optionalDoubleField(row, "sugar_g");
    dto.sodiumMg = optionalDoubleField(row, "sodium_mg");

    // 출처/제조/원산지 정보(optional)
    dto.sourceName = optionalStringField(row, "source_name");
    dto.manufacturerName = optionalStringField(row, "manufacturer_name");
    dto.importYn = optionalStringField(row, "import_yn");
    dto.originCountryName = optionalStringField(row, "origin_country_name");
    // DATE 컬럼도 SELECT에서 문자열 포맷팅했으므로 optional<string>로 받는다.
    dto.dataBaseDate = optionalStringField(row, "data_base_date");

    // 메타 시각 정보
    // created_at / updated_at은 COALESCE로 빈 문자열 보정되어 null 예외 없이 변환 가능하다.
    dto.createdAt = row["created_at"].as<std::string>();
    dto.updatedAt = row["updated_at"].as<std::string>();
    return dto;
}

void IngredientMapper::ensureTablesForLocalDev() const
{
    // 프로세스 생애주기 동안 DDL을 1회만 실행하기 위한 플래그.
    // 로컬 환경에서만 필요한 안전장치이며, 이미 준비되었으면 즉시 반환한다.
    static std::atomic_bool tablesReady{false};
    if (tablesReady.load())
    {
        return;
    }

    const auto dbClient = drogon::app().getDbClient("default");

    // ingredients의 FK 대상이 되는 members 테이블을 먼저 보장한다.
    // FK 생성 순서를 지키지 않으면 ingredients 테이블 생성이 실패할 수 있다.
    dbClient->execSqlSync(
        "CREATE TABLE IF NOT EXISTS members ("
        "member_id BIGINT UNSIGNED NOT NULL AUTO_INCREMENT,"
        "email VARCHAR(255) NOT NULL,"
        "password_hash VARCHAR(255) NOT NULL,"
        "name VARCHAR(100) NOT NULL,"
        "role VARCHAR(20) NOT NULL DEFAULT 'USER',"
        "status VARCHAR(20) NOT NULL DEFAULT 'ACTIVE',"
        "level INT UNSIGNED NOT NULL DEFAULT 1,"
        "exp INT UNSIGNED NOT NULL DEFAULT 0,"
        "last_login_at DATETIME NULL,"
        "created_at DATETIME NOT NULL DEFAULT CURRENT_TIMESTAMP,"
        "updated_at DATETIME NOT NULL DEFAULT CURRENT_TIMESTAMP ON UPDATE "
        "CURRENT_TIMESTAMP,"
        "PRIMARY KEY (member_id),"
        "UNIQUE KEY uq_members_email (email),"
        "KEY idx_members_status (status)"
        ") ENGINE=InnoDB DEFAULT CHARSET=utf8mb4");

    // 로컬 실행에 필요한 ingredients 최소 스키마를 자동 생성한다.
    // 인덱스는 회원별 조회/정렬/삭제 필터 조건에 맞춰 기본 성능을 확보한다.
    dbClient->execSqlSync(
        "CREATE TABLE IF NOT EXISTS ingredients ("
        "ingredient_id BIGINT UNSIGNED NOT NULL AUTO_INCREMENT,"
        "member_id BIGINT UNSIGNED NOT NULL,"
        "public_food_code VARCHAR(50) NULL,"
        "name VARCHAR(150) NOT NULL,"
        "category VARCHAR(50) NOT NULL,"
        "quantity VARCHAR(50) NOT NULL,"
        "unit VARCHAR(20) NOT NULL,"
        "storage ENUM('FRIDGE', 'FREEZER', 'ROOM_TEMP', 'OTHER') NOT NULL,"
        "expiry_date DATE NOT NULL,"
        "nutrition_basis_amount VARCHAR(50) NULL,"
        "energy_kcal DECIMAL(10,2) NULL,"
        "protein_g DECIMAL(10,2) NULL,"
        "fat_g DECIMAL(10,2) NULL,"
        "carbohydrate_g DECIMAL(10,2) NULL,"
        "sugar_g DECIMAL(10,2) NULL,"
        "sodium_mg DECIMAL(10,2) NULL,"
        "source_name VARCHAR(100) NULL,"
        "manufacturer_name VARCHAR(150) NULL,"
        "import_yn ENUM('Y', 'N') NULL,"
        "origin_country_name VARCHAR(100) NULL,"
        "data_base_date DATE NULL,"
        "created_at DATETIME NOT NULL DEFAULT CURRENT_TIMESTAMP,"
        "updated_at DATETIME NOT NULL DEFAULT CURRENT_TIMESTAMP ON UPDATE CURRENT_TIMESTAMP,"
        "deleted_at DATETIME NULL,"
        "PRIMARY KEY (ingredient_id),"
        "KEY idx_ingredients_member_id (member_id),"
        "KEY idx_ingredients_member_storage (member_id, storage),"
        "KEY idx_ingredients_member_expiry_date (member_id, expiry_date),"
        "KEY idx_ingredients_public_food_code (public_food_code),"
        "KEY idx_ingredients_member_deleted_at (member_id, deleted_at),"
        "CONSTRAINT fk_ingredients_member FOREIGN KEY (member_id) "
        "REFERENCES members(member_id) ON DELETE CASCADE"
        ") ENGINE=InnoDB DEFAULT CHARSET=utf8mb4");

    // 이후 호출에서는 DDL을 건너뛰어 런타임 오버헤드를 제거한다.
    tablesReady.store(true);
}

}  // namespace ingredient
