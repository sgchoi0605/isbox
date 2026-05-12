#pragma once

#include <functional>
#include <optional>
#include <string>

#include "../models/IngredientTypes.h"

namespace ingredient
{

// Public nutrition API based processed-food search service.
class ProcessedFoodSearchService
{
  public:
    using ProcessedFoodSearchCallback =
        std::function<void(ProcessedFoodSearchResultDTO)>;

    ProcessedFoodSearchService() = default;

    void searchProcessedFoods(const std::string &keyword,
                              ProcessedFoodSearchCallback &&callback);

  private:
    static std::string trim(std::string value);
    static std::string normalizeForSearch(std::string value);
    static std::optional<std::string> getEnvValue(const char *key);
    static std::optional<std::string> getServiceKeyFromLocalFile();
};

}  // namespace ingredient
