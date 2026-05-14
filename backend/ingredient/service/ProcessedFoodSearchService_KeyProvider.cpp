/*
 * 파일 개요: 공공 API 서비스 키 조회 제공자 구현 파일이다.
 * 주요 역할: 환경변수 우선 조회, 로컬 키 파일 탐색/파싱, 키 값 캐싱으로 반복 I/O를 줄이는 로직을 수행한다.
 * 사용 위치: ProcessedFoodSearchService와 NutritionFoodsSearchService가 API 호출 전 인증키를 확보할 때 사용한다.
 */
#include "ingredient/service/ProcessedFoodSearchService_KeyProvider.h"

#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <vector>

#include <json/reader.h>

#include "ingredient/service/ProcessedFoodSearchService_Text.h"

namespace ingredient
{

// 환경변수 값이 비어 있으면 미설정으로 취급한다.
std::optional<std::string> ProcessedFoodSearchService_KeyProvider::getEnvValue(
    const char *key)
{
    const auto *raw = std::getenv(key);
    if (raw == nullptr)
    {
        return std::nullopt;
    }

    const std::string value(raw);
    if (value.empty())
    {
        return std::nullopt;
    }
    return value;
}

// 여러 실행 경로를 고려해 keys.local.json 후보를 순회한다.
std::optional<std::string> ProcessedFoodSearchService_KeyProvider::getServiceKeyFromLocalFile()
{
    std::lock_guard<std::mutex> lock(cacheMutex_);
    if (cached_.has_value())
    {
        return cached_;
    }

    const std::vector<std::filesystem::path> candidates = {
        std::filesystem::path("config") / "keys.local.json",
        std::filesystem::path("backend") / "config" / "keys.local.json",
        std::filesystem::path("..") / "config" / "keys.local.json",
        std::filesystem::path("..") / ".." / "config" / "keys.local.json",
        std::filesystem::path("..") / ".." / ".." / "config" /
            "keys.local.json",
    };

    for (const auto &path : candidates)
    {
        if (!std::filesystem::exists(path))
        {
            continue;
        }

        std::ifstream file(path);
        if (!file.is_open())
        {
            continue;
        }

        Json::Value root;
        Json::CharReaderBuilder builder;
        std::string errors;
        if (!Json::parseFromStream(builder, file, &root, &errors))
        {
            continue;
        }

        if (!root.isObject() || !root["publicNutriServiceKey"].isString())
        {
            continue;
        }

        auto value = ProcessedFoodSearchService_Text::trim(
            root["publicNutriServiceKey"].asString());
        if (value.empty())
        {
            continue;
        }

        // 한 번 찾은 키는 캐시해 반복 파일 I/O를 피한다.
        cached_ = value;
        return cached_;
    }

    return std::nullopt;
}

// 우선순위: 환경변수 -> 로컬 파일.
std::optional<std::string> ProcessedFoodSearchService_KeyProvider::getServiceKey()
{
    auto serviceKey = getEnvValue("PUBLIC_NUTRI_SERVICE_KEY");
    if (serviceKey.has_value())
    {
        return serviceKey;
    }

    return getServiceKeyFromLocalFile();
}

}  // namespace ingredient


