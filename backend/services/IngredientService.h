#pragma once

#include <cstdint>
#include <optional>
#include <string>

#include "../mappers/IngredientMapper.h"
#include "../models/IngredientTypes.h"

namespace ingredient
{

// Ingredient domain service. Handles validation and orchestration.
class IngredientService
{
  public:
    IngredientService() = default;

    IngredientListResultDTO getIngredients(std::uint64_t memberId);

    IngredientSingleResultDTO createIngredient(
        std::uint64_t memberId,
        const CreateIngredientRequestDTO &request);

    IngredientSingleResultDTO updateIngredient(
        std::uint64_t memberId,
        std::uint64_t ingredientId,
        const UpdateIngredientRequestDTO &request);

    IngredientDeleteResultDTO deleteIngredient(std::uint64_t memberId,
                                               std::uint64_t ingredientId);

  private:
    static std::string trim(std::string value);
    static std::string toLower(std::string value);
    static bool isValidDate(const std::string &value);
    static bool isAllowedClientStorage(const std::string &value);
    static std::string toDbStorage(const std::string &clientValue);
    static std::string toClientStorage(const std::string &dbValue);
    static std::optional<std::string> normalizeImportYn(
        const std::optional<std::string> &value);
    static void normalizeIngredientForClient(IngredientDTO &ingredient);

    IngredientMapper mapper_;
};

}  // namespace ingredient
