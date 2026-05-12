#include "PublicNutritionFoodMapper.h"

#include <drogon/drogon.h>

#include <algorithm>
#include <atomic>
#include <cctype>
#include <iomanip>
#include <sstream>
#include <unordered_map>
#include <utility>

namespace
{

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

std::string optionalStringToInsertValue(const std::optional<std::string> &value)
{
    if (!value.has_value())
    {
        return "";
    }
    return *value;
}

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

std::string toSortableDate(const std::optional<std::string> &value)
{
    if (!value.has_value())
    {
        return "";
    }
    return *value;
}

}  // namespace

namespace ingredient
{

std::size_t PublicNutritionFoodMapper::countFoods() const
{
    ensureTableForLocalDev();

    const auto dbClient = drogon::app().getDbClient("default");
    const auto result =
        dbClient->execSqlSync("SELECT COUNT(*) AS cnt FROM public_nutrition_foods");
    if (result.empty())
    {
        return 0U;
    }

    return static_cast<std::size_t>(result[0]["cnt"].as<std::uint64_t>());
}

void PublicNutritionFoodMapper::upsertFoods(
    const std::vector<ProcessedFoodSearchItemDTO> &foods) const
{
    ensureTableForLocalDev();

    if (foods.empty())
    {
        return;
    }

    const auto dbClient = drogon::app().getDbClient("default");
    for (const auto &food : foods)
    {
        if (food.foodCode.empty() || food.foodName.empty())
        {
            continue;
        }

        const auto normalizedFoodName = normalizeForSearch(food.foodName);
        if (normalizedFoodName.empty())
        {
            continue;
        }

        dbClient->execSqlSync(
            "INSERT INTO public_nutrition_foods ("
            "food_code, food_name, normalized_food_name, food_group_name, "
            "nutrition_basis_amount, energy_kcal, protein_g, fat_g, carbohydrate_g, sugar_g, sodium_mg, "
            "source_name, manufacturer_name, import_yn, origin_country_name, data_base_date, updated_at"
            ") VALUES ("
            "?, ?, ?, NULLIF(?, ''), "
            "NULLIF(?, ''), NULLIF(?, ''), NULLIF(?, ''), NULLIF(?, ''), NULLIF(?, ''), NULLIF(?, ''), NULLIF(?, ''), "
            "NULLIF(?, ''), NULLIF(?, ''), NULLIF(?, ''), NULLIF(?, ''), NULLIF(?, ''), NOW()"
            ") "
            "ON DUPLICATE KEY UPDATE "
            "food_name = VALUES(food_name), "
            "normalized_food_name = VALUES(normalized_food_name), "
            "food_group_name = VALUES(food_group_name), "
            "nutrition_basis_amount = VALUES(nutrition_basis_amount), "
            "energy_kcal = VALUES(energy_kcal), "
            "protein_g = VALUES(protein_g), "
            "fat_g = VALUES(fat_g), "
            "carbohydrate_g = VALUES(carbohydrate_g), "
            "sugar_g = VALUES(sugar_g), "
            "sodium_mg = VALUES(sodium_mg), "
            "source_name = VALUES(source_name), "
            "manufacturer_name = VALUES(manufacturer_name), "
            "import_yn = VALUES(import_yn), "
            "origin_country_name = VALUES(origin_country_name), "
            "data_base_date = VALUES(data_base_date), "
            "updated_at = NOW()",
            food.foodCode,
            food.foodName,
            normalizedFoodName,
            optionalStringToInsertValue(food.foodGroupName),
            optionalStringToInsertValue(food.nutritionBasisAmount),
            optionalDoubleToInsertValue(food.energyKcal),
            optionalDoubleToInsertValue(food.proteinG),
            optionalDoubleToInsertValue(food.fatG),
            optionalDoubleToInsertValue(food.carbohydrateG),
            optionalDoubleToInsertValue(food.sugarG),
            optionalDoubleToInsertValue(food.sodiumMg),
            optionalStringToInsertValue(food.sourceName),
            optionalStringToInsertValue(food.manufacturerName),
            optionalStringToInsertValue(food.importYn),
            optionalStringToInsertValue(food.originCountryName),
            optionalStringToInsertValue(food.dataBaseDate));
    }
}

std::vector<ProcessedFoodSearchItemDTO> PublicNutritionFoodMapper::searchByKeyword(
    const std::string &keyword) const
{
    ensureTableForLocalDev();

    const auto normalizedKeyword = normalizeForSearch(keyword);
    if (normalizedKeyword.empty())
    {
        return {};
    }

    const auto dbClient = drogon::app().getDbClient("default");
    const auto rows = dbClient->execSqlSync(
        "SELECT "
        "food_code, food_name, food_group_name, "
        "nutrition_basis_amount, energy_kcal, protein_g, fat_g, carbohydrate_g, sugar_g, sodium_mg, "
        "source_name, manufacturer_name, import_yn, origin_country_name, "
        "DATE_FORMAT(data_base_date, '%Y-%m-%d') AS data_base_date "
        "FROM public_nutrition_foods "
        "WHERE normalized_food_name LIKE CONCAT('%', ?, '%')",
        normalizedKeyword);

    std::unordered_map<std::string, ProcessedFoodSearchItemDTO> representativeByName;
    representativeByName.reserve(rows.size());

    for (const auto &row : rows)
    {
        auto dto = rowToDTO(row);
        if (dto.foodName.empty())
        {
            continue;
        }

        auto it = representativeByName.find(dto.foodName);
        if (it == representativeByName.end())
        {
            representativeByName.emplace(dto.foodName, std::move(dto));
            continue;
        }

        if (isRepresentativeBetter(dto, it->second))
        {
            it->second = std::move(dto);
        }
    }

    std::vector<ProcessedFoodSearchItemDTO> deduped;
    deduped.reserve(representativeByName.size());
    for (auto &entry : representativeByName)
    {
        deduped.push_back(std::move(entry.second));
    }

    std::sort(
        deduped.begin(),
        deduped.end(),
        [&normalizedKeyword](const ProcessedFoodSearchItemDTO &lhs,
                             const ProcessedFoodSearchItemDTO &rhs) {
            const auto lp = searchPriority(lhs, normalizedKeyword);
            const auto rp = searchPriority(rhs, normalizedKeyword);
            if (lp != rp)
            {
                return lp < rp;
            }
            return lhs.foodName < rhs.foodName;
        });

    return deduped;
}

std::string PublicNutritionFoodMapper::normalizeForSearch(std::string value)
{
    value.erase(std::remove_if(value.begin(),
                               value.end(),
                               [](unsigned char ch) {
                                   return std::isspace(ch) != 0;
                               }),
                value.end());

    std::transform(value.begin(), value.end(), value.begin(), [](unsigned char ch) {
        return static_cast<char>(std::tolower(ch));
    });

    return value;
}

ProcessedFoodSearchItemDTO PublicNutritionFoodMapper::rowToDTO(
    const drogon::orm::Row &row)
{
    ProcessedFoodSearchItemDTO dto;
    dto.foodCode = row["food_code"].as<std::string>();
    dto.foodName = row["food_name"].as<std::string>();
    dto.foodGroupName = optionalStringField(row, "food_group_name");
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
    return dto;
}

int PublicNutritionFoodMapper::nutritionFilledFieldCount(
    const ProcessedFoodSearchItemDTO &food)
{
    int count = 0;
    if (food.nutritionBasisAmount.has_value())
    {
        ++count;
    }
    if (food.energyKcal.has_value())
    {
        ++count;
    }
    if (food.proteinG.has_value())
    {
        ++count;
    }
    if (food.fatG.has_value())
    {
        ++count;
    }
    if (food.carbohydrateG.has_value())
    {
        ++count;
    }
    if (food.sugarG.has_value())
    {
        ++count;
    }
    if (food.sodiumMg.has_value())
    {
        ++count;
    }
    return count;
}

bool PublicNutritionFoodMapper::isRepresentativeBetter(
    const ProcessedFoodSearchItemDTO &lhs,
    const ProcessedFoodSearchItemDTO &rhs)
{
    const auto lhsDate = toSortableDate(lhs.dataBaseDate);
    const auto rhsDate = toSortableDate(rhs.dataBaseDate);
    if (lhsDate != rhsDate)
    {
        return lhsDate > rhsDate;
    }

    const auto lhsNutritionCount = nutritionFilledFieldCount(lhs);
    const auto rhsNutritionCount = nutritionFilledFieldCount(rhs);
    if (lhsNutritionCount != rhsNutritionCount)
    {
        return lhsNutritionCount > rhsNutritionCount;
    }

    return lhs.foodCode < rhs.foodCode;
}

int PublicNutritionFoodMapper::searchPriority(
    const ProcessedFoodSearchItemDTO &food,
    const std::string &normalizedKeyword)
{
    const auto normalizedFoodName = normalizeForSearch(food.foodName);
    if (normalizedFoodName == normalizedKeyword)
    {
        return 0;
    }

    if (normalizedFoodName.size() >= normalizedKeyword.size() &&
        normalizedFoodName.compare(0, normalizedKeyword.size(), normalizedKeyword) ==
            0)
    {
        return 1;
    }

    return 2;
}

void PublicNutritionFoodMapper::ensureTableForLocalDev() const
{
    static std::atomic_bool tableReady{false};
    if (tableReady.load())
    {
        return;
    }

    const auto dbClient = drogon::app().getDbClient("default");
    dbClient->execSqlSync(
        "CREATE TABLE IF NOT EXISTS public_nutrition_foods ("
        "food_code VARCHAR(50) NOT NULL,"
        "food_name VARCHAR(255) NOT NULL,"
        "normalized_food_name VARCHAR(255) NOT NULL,"
        "food_group_name VARCHAR(255) NULL,"
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
        "updated_at DATETIME NOT NULL DEFAULT CURRENT_TIMESTAMP ON UPDATE CURRENT_TIMESTAMP,"
        "PRIMARY KEY (food_code),"
        "UNIQUE KEY uq_public_nutrition_food_code (food_code),"
        "KEY idx_public_nutrition_food_name (food_name),"
        "KEY idx_public_nutrition_normalized_food_name (normalized_food_name)"
        ") ENGINE=InnoDB DEFAULT CHARSET=utf8mb4");

    tableReady.store(true);
}

}  // namespace ingredient

