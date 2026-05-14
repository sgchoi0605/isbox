/*
 * 파일 개요: IngredientMapper.h의 DB 쿼리와 row 매핑 로직을 구현한 파일.
 * 주요 역할: SQL 실행, NULL 안전 변환, DTO 조립, 동시성 환경에서의 식별자 생성 보조를 수행한다.
 * 동작 특징: DB 스키마 표현을 애플리케이션 DTO 표현으로 일관되게 매핑한다.
 */

// IngredientMapper 구현 파일.
#include "ingredient/mapper/IngredientMapper.h"

// Drogon ORM/DB 클라이언트 사용.
#include <drogon/drogon.h>

#include <atomic>
#include <iomanip>
#include <sstream>

namespace
{

// 문자열 컬럼을 optional<string>으로 안전하게 읽는다.
std::optional<std::string> optionalStringField(const drogon::orm::Row &row,
                                               const char *column)
{
    const auto &field = row[column];
    if (field.isNull())
    {
        return std::nullopt;
    }

    const auto value = field.as<std::string>();
    if (value.empty())
    {
        return std::nullopt;
    }
    return value;
}

// 숫자 컬럼을 optional<double>로 안전하게 읽는다.
std::optional<double> optionalDoubleField(const drogon::orm::Row &row,
                                          const char *column)
{
    const auto &field = row[column];
    if (field.isNull())
    {
        return std::nullopt;
    }
    return field.as<double>();
}

// optional 문자열을 INSERT 바인딩값으로 바꾼다.
// 값 없음은 빈 문자열로 전달하고 SQL NULLIF(?, '')에서 NULL로 변환한다.
std::string optionalStringToInsertValue(const std::optional<std::string> &value)
{
    if (!value.has_value())
    {
        return "";
    }
    return *value;
}

// optional 숫자를 INSERT 바인딩값 문자열로 바꾼다.
// 값 없음은 빈 문자열로 전달하고 SQL NULLIF(?, '')에서 NULL로 변환한다.
std::string optionalDoubleToInsertValue(const std::optional<double> &value)
{
    if (!value.has_value())
    {
        return "";
    }

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
    // 로컬 개발 환경에서 필요한 테이블을 보장한다.
    ensureTablesForLocalDev();

    const auto dbClient = drogon::app().getDbClient("default");
    // 삭제되지 않은 사용자 식재료를 storage/expiry 우선순위로 조회한다.
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

    // 결과 row를 DTO 목록으로 변환한다.
    std::vector<IngredientDTO> ingredients;
    ingredients.reserve(result.size());
    for (const auto &row : result)
    {
        ingredients.push_back(rowToIngredientDTO(row));
    }
    return ingredients;
}

std::optional<IngredientDTO> IngredientMapper::findByIdForMember(
    std::uint64_t ingredientId,
    std::uint64_t memberId) const
{
    // 로컬 개발 환경에서 필요한 테이블을 보장한다.
    ensureTablesForLocalDev();

    const auto dbClient = drogon::app().getDbClient("default");
    // 소유권 확인까지 포함해 단건을 조회한다.
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

    if (result.empty())
    {
        return std::nullopt;
    }

    return rowToIngredientDTO(result[0]);
}

std::optional<IngredientDTO> IngredientMapper::insertIngredient(
    std::uint64_t memberId,
    const CreateIngredientRequestDTO &request) const
{
    // 로컬 개발 환경에서 필요한 테이블을 보장한다.
    ensureTablesForLocalDev();

    // optional 값은 NULLIF(?, '') 패턴으로 NULL 저장을 통일한다.
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

    const auto insertedId = insertResult.insertId();
    if (insertedId == 0ULL)
    {
        return std::nullopt;
    }

    // DB 기본값(created_at/updated_at 등)을 포함한 최종 row를 반환한다.
    return findByIdForMember(insertedId, memberId);
}

bool IngredientMapper::updateEditableFields(
    std::uint64_t ingredientId,
    std::uint64_t memberId,
    const UpdateIngredientRequestDTO &request) const
{
    // 로컬 개발 환경에서 필요한 테이블을 보장한다.
    ensureTablesForLocalDev();

    const auto dbClient = drogon::app().getDbClient("default");
    // 사용자 소유 데이터만 UPDATE 되도록 WHERE 절에 member_id를 포함한다.
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

    return result.affectedRows() > 0;
}

bool IngredientMapper::markDeleted(std::uint64_t ingredientId,
                                   std::uint64_t memberId) const
{
    // 로컬 개발 환경에서 필요한 테이블을 보장한다.
    ensureTablesForLocalDev();

    const auto dbClient = drogon::app().getDbClient("default");
    // 현재 구현은 하드 삭제를 수행한다(요구사항 변경 시 soft delete로 교체 가능).
    const auto result = dbClient->execSqlSync(
        "DELETE FROM ingredients "
        "WHERE ingredient_id = ? AND member_id = ?",
        ingredientId,
        memberId);

    return result.affectedRows() > 0;
}

IngredientDTO IngredientMapper::rowToIngredientDTO(const drogon::orm::Row &row)
{
    // DB row를 API 응답 DTO로 매핑한다.
    IngredientDTO dto;

    dto.ingredientId = row["ingredient_id"].as<std::uint64_t>();
    dto.memberId = row["member_id"].as<std::uint64_t>();
    dto.publicFoodCode = optionalStringField(row, "public_food_code");
    dto.name = row["name"].as<std::string>();
    dto.category = row["category"].as<std::string>();
    dto.quantity = row["quantity"].as<std::string>();
    dto.unit = row["unit"].as<std::string>();
    dto.storage = row["storage"].as<std::string>();
    dto.expiryDate = row["expiry_date"].as<std::string>();

    dto.nutritionBasisAmount = optionalStringField(row, "nutrition_basis_amount");
    dto.energyKcal = optionalDoubleField(row, "energy_kcal");
    dto.proteinG = optionalDoubleField(row, "protein_g");
    dto.fatG = optionalDoubleField(row, "fat_g");
    dto.carbohydrateG = optionalDoubleField(row, "carbohydrate_g");
    dto.sugarG = optionalDoubleField(row, "sugar_g");
    dto.sodiumMg = optionalDoubleField(row, "sodium_mg");

    dto.sourceName = optionalStringField(row, "source_name");
    dto.manufacturerName = optionalStringField(row, "manufacturer_name");
    dto.importYn = optionalStringField(row, "import_yn");
    dto.originCountryName = optionalStringField(row, "origin_country_name");
    dto.dataBaseDate = optionalStringField(row, "data_base_date");

    dto.createdAt = row["created_at"].as<std::string>();
    dto.updatedAt = row["updated_at"].as<std::string>();
    return dto;
}

void IngredientMapper::ensureTablesForLocalDev() const
{
    // 테이블 생성 DDL은 프로세스당 한 번만 수행한다.
    static std::atomic_bool tablesReady{false};
    if (tablesReady.load())
    {
        return;
    }

    const auto dbClient = drogon::app().getDbClient("default");

    // FK 대상인 members 테이블을 먼저 보장한다.
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

    // ingredients 테이블과 인덱스를 보장한다.
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

    // 모든 DDL이 완료되면 재진입 시 빠르게 return 하도록 플래그를 올린다.
    tablesReady.store(true);
}

}  // namespace ingredient



