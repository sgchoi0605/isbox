#include "IngredientService.h"

#include <algorithm>
#include <cctype>
#include <exception>
#include <regex>

namespace
{

// JSON 媛믪쓣 鍮꾩뼱 ?덉? ?딆? ?좏깮 臾몄옄?대줈 蹂?섑븳??
std::optional<std::string> normalizeDataBaseDate(
    const std::optional<std::string> &value)
{
    // 怨듦났 API 湲곗??쇱옄??"20240512"泥섎읆 遺숈뼱???ㅺ린???섍퀬 ?대? 援щ텇?먭? ?덇린???섎떎.
    // 鍮?媛믪? ??ν븯吏 ?딄퀬, 8?먮━ ?レ옄留??쒕퉬???쒖? ?좎쭨 ?뺤떇?쇰줈 蹂?섑븳??
    if (!value.has_value())
    {
        return std::nullopt;
    }

    std::string text = *value;
    text.erase(
        std::remove_if(text.begin(), text.end(), [](unsigned char ch) {
            return std::isspace(ch) != 0;
        }),
        text.end());

    if (text.empty())
    {
        return std::nullopt;
    }

    static const std::regex yyyymmdd("^[0-9]{8}$");
    if (std::regex_match(text, yyyymmdd))
    {
        return text.substr(0, 4) + "-" + text.substr(4, 2) + "-" +
               text.substr(6, 2);
    }

    return text;
}
}  // namespace

namespace ingredient
{

IngredientListResultDTO IngredientService::getIngredients(std::uint64_t memberId)
{
    IngredientListResultDTO result;

    // ?몄뀡???녿뒗 ?붿껌? ?몄쬆?섏? ?딆? ?붿껌?쇰줈 泥섎━?쒕떎.
    if (memberId == 0)
    {
        result.statusCode = 401;
        result.message = "Unauthorized.";
        return result;
    }

    try
    {
        // ?뚯썝蹂??앹옱猷?議고쉶??留ㅽ띁媛 ?대떦?쒕떎.
        // ?쒕퉬?ㅻ뒗 議고쉶 寃곌낵瑜??대씪?댁뼵???묐떟 怨꾩빟??留욊쾶 ?꾩쿂由ы븳??
        result.ingredients = mapper_.findIngredientsByMemberId(memberId);

        // ?묐떟 吏곷젹???꾩뿉 ?닿굅?뺢낵 ?좎쭨 媛믪쓣 ?대씪?댁뼵???뺤떇?쇰줈 ?뺢퇋?뷀븳??
        for (auto &ingredient : result.ingredients)
        {
            normalizeIngredientForClient(ingredient);
        }

        result.ok = true;
        result.statusCode = 200;
        result.message = "Ingredients loaded.";
        return result;
    }
    catch (const std::exception &)
    {
        result.statusCode = 500;
        result.message = "Server error while loading ingredients.";
        return result;
    }
}

IngredientSingleResultDTO IngredientService::createIngredient(
    std::uint64_t memberId,
    const CreateIngredientRequestDTO &request)
{
    IngredientSingleResultDTO result;

    // ?몄뀡???녿뒗 ?붿껌? ?몄쬆?섏? ?딆? ?붿껌?쇰줈 泥섎━?쒕떎.
    if (memberId == 0)
    {
        result.statusCode = 401;
        result.message = "Unauthorized.";
        return result;
    }

    // ?ъ슜?먭? 吏곸젒 ?낅젰?섎뒗 ?듭떖 ?꾨뱶??癒쇱? 怨듬갚???쒓굅?쒕떎.
    // storage????뚮Ц?먮? ?덉슜?섍린 ?꾪빐 ?뚮Ц??湲곗??쇰줈 寃利앺븳??
    const auto name = trim(request.name);
    const auto category = trim(request.category);
    const auto quantity = trim(request.quantity);
    const auto unit = trim(request.unit);
    const auto storage = toLower(trim(request.storage));
    const auto expiryDate = trim(request.expiryDate);

    // ?대쫫, 移댄뀒怨좊━, ?섎웾, ?⑥쐞, 蹂닿? ?꾩튂, ?좏넻湲고븳? ?앹꽦 ??諛섎뱶???꾩슂?섎떎.
    // 怨듬갚留??낅젰??媛믪? trim ?댄썑 鍮?媛믪쑝濡?泥섎━?쒕떎.
    if (name.empty() || category.empty() || quantity.empty() || unit.empty() ||
        storage.empty() || expiryDate.empty())
    {
        result.statusCode = 400;
        result.message = "Required ingredient fields are missing.";
        return result;
    }

    // DB 而щ읆 ?ш린? UI ?낅젰 ?쒗븳??留욎텛湲??꾪빐 臾몄옄??湲몄씠瑜??쒕퉬?ㅼ뿉??諛⑹뼱?쒕떎.
    if (name.size() > 150U || category.size() > 50U || quantity.size() > 50U ||
        unit.size() > 20U)
    {
        result.statusCode = 400;
        result.message = "Ingredient field length exceeds limit.";
        return result;
    }

    // 蹂닿? ?꾩튂 媛믪? ?덉슜???대씪?댁뼵???닿굅?뺤씠?댁빞 ?쒕떎.
    if (!isAllowedClientStorage(storage))
    {
        result.statusCode = 400;
        result.message = "Invalid storage value.";
        return result;
    }

    // ?좎쭨??YYYY-MM-DD ?뺤떇?댁뼱???쒕떎.
    if (!isValidDate(expiryDate))
    {
        result.statusCode = 400;
        result.message = "Invalid expiry date format.";
        return result;
    }

    // ?먮낯 request瑜?蹂듭궗???? 寃利앸맂 ?듭떖 ?꾨뱶留??뺢퇋?붾맂 媛믪쑝濡???뼱?대떎.
    // ?대젃寃??섎㈃ 怨듦났 API 硫뷀??곗씠??媛숈? ?좏깮 ?꾨뱶???좎??섎㈃?쒕룄 ?듭떖 ?꾨뱶???좊ː?????덈떎.
    CreateIngredientRequestDTO normalized = request;
    normalized.name = name;
    normalized.category = category;
    normalized.quantity = quantity;
    normalized.unit = unit;
    normalized.storage = toDbStorage(storage);
    normalized.expiryDate = expiryDate;

    // 怨듦났 API?먯꽌 媛?몄삩 ?좏깮 硫뷀??곗씠?곕뒗 ?ъ슜?먭? 吏곸젒 ?낅젰?섏? ?딆쓣 ?섎룄 ?덈떎.
    // 怨듬갚 臾몄옄?댁쓣 ??ν븯吏 ?딅룄濡?媛??꾨뱶瑜?trim ??鍮꾩뼱 ?덉쑝硫?nullopt濡?諛붽씔??
    if (normalized.publicFoodCode.has_value())
    {
        normalized.publicFoodCode = trim(*normalized.publicFoodCode);
        if (normalized.publicFoodCode->empty())
        {
            normalized.publicFoodCode = std::nullopt;
        }
    }
    if (normalized.nutritionBasisAmount.has_value())
    {
        normalized.nutritionBasisAmount = trim(*normalized.nutritionBasisAmount);
        if (normalized.nutritionBasisAmount->empty())
        {
            normalized.nutritionBasisAmount = std::nullopt;
        }
    }
    if (normalized.sourceName.has_value())
    {
        normalized.sourceName = trim(*normalized.sourceName);
        if (normalized.sourceName->empty())
        {
            normalized.sourceName = std::nullopt;
        }
    }
    if (normalized.manufacturerName.has_value())
    {
        normalized.manufacturerName = trim(*normalized.manufacturerName);
        if (normalized.manufacturerName->empty())
        {
            normalized.manufacturerName = std::nullopt;
        }
    }
    if (normalized.originCountryName.has_value())
    {
        normalized.originCountryName = trim(*normalized.originCountryName);
        if (normalized.originCountryName->empty())
        {
            normalized.originCountryName = std::nullopt;
        }
    }

    // ?쒗븳??媛?吏묓빀??媛뽯뒗 ?좏깮 ?꾨뱶瑜??뺢퇋?뷀븳??
    // importYn? Y/N留??덉슜?섍퀬, 湲곗??쇱옄??YYYYMMDD ?뺤떇???쒕퉬???좎쭨 ?뺤떇?쇰줈 留욎텣??
    normalized.importYn = normalizeImportYn(normalized.importYn);
    normalized.dataBaseDate = normalizeDataBaseDate(normalized.dataBaseDate);

    try
    {
        // 留ㅽ띁???대? ?뺢퇋?붾맂 媛믪쓣 諛쏆븘 DB insert留??섑뻾?쒕떎.
        auto created = mapper_.insertIngredient(memberId, normalized);
        if (!created.has_value())
        {
            result.statusCode = 500;
            result.message = "Failed to create ingredient.";
            return result;
        }

        // ?앹꽦 吏곹썑 ?묐떟??紐⑸줉 議고쉶? 媛숈? ?대씪?댁뼵???뺤떇?쇰줈 留욎텣??
        normalizeIngredientForClient(*created);

        result.ok = true;
        result.statusCode = 201;
        result.message = "Ingredient created.";
        result.ingredient = created;
        return result;
    }
    catch (const std::exception &)
    {
        result.statusCode = 500;
        result.message = "Server error while creating ingredient.";
        return result;
    }
}

IngredientSingleResultDTO IngredientService::updateIngredient(
    std::uint64_t memberId,
    std::uint64_t ingredientId,
    const UpdateIngredientRequestDTO &request)
{
    IngredientSingleResultDTO result;

    // ?몄뀡???녿뒗 ?붿껌? ?몄쬆?섏? ?딆? ?붿껌?쇰줈 泥섎━?쒕떎.
    if (memberId == 0)
    {
        result.statusCode = 401;
        result.message = "Unauthorized.";
        return result;
    }

    // ???id???꾩닔??
    if (ingredientId == 0)
    {
        result.statusCode = 400;
        result.message = "Ingredient id is required.";
        return result;
    }

    // ?섏젙 API???앹꽦 API? 媛숈? ?듭떖 ?꾨뱶 ?명듃瑜?諛쏅뒗??
    // 怨듬갚 ?쒓굅? storage ?뚮Ц??蹂?섏쓣 癒쇱? ?곸슜??寃利?湲곗????듭씪?쒕떎.
    const auto name = trim(request.name);
    const auto category = trim(request.category);
    const auto quantity = trim(request.quantity);
    const auto unit = trim(request.unit);
    const auto storage = toLower(trim(request.storage));
    const auto expiryDate = trim(request.expiryDate);

    // ?꾩옱 ?섏젙? 遺遺??낅뜲?댄듃媛 ?꾨땲???꾩껜 ?섏젙 諛⑹떇?대떎.
    // ?곕씪???앹꽦 ?뚯? 媛숈? ?꾩닔 ?꾨뱶瑜?紐⑤몢 ?붽뎄?쒕떎.
    if (name.empty() || category.empty() || quantity.empty() || unit.empty() ||
        storage.empty() || expiryDate.empty())
    {
        result.statusCode = 400;
        result.message = "Required ingredient fields are missing.";
        return result;
    }

    // 臾몄옄???꾨뱶 湲몄씠 ?쒗븳???뺤씤?쒕떎.
    if (name.size() > 150U || category.size() > 50U || quantity.size() > 50U ||
        unit.size() > 20U)
    {
        result.statusCode = 400;
        result.message = "Ingredient field length exceeds limit.";
        return result;
    }

    // 蹂닿? ?꾩튂 媛믪? ?덉슜???대씪?댁뼵???닿굅?뺤씠?댁빞 ?쒕떎.
    if (!isAllowedClientStorage(storage))
    {
        result.statusCode = 400;
        result.message = "Invalid storage value.";
        return result;
    }

    // ?좎쭨??YYYY-MM-DD ?뺤떇?댁뼱???쒕떎.
    if (!isValidDate(expiryDate))
    {
        result.statusCode = 400;
        result.message = "Invalid expiry date format.";
        return result;
    }

    try
    {
        // ????앹옱猷뚮뒗 ?꾩옱 ?뚯썝???뚯쑀?ъ빞 ?쒕떎.
        // ???뺤씤?쇰줈 ?ㅻⅨ ?뚯썝???앹옱猷?id瑜?吏곸젒 ?몄텧?섎뒗 ?붿껌??李⑤떒?쒕떎.
        const auto existing = mapper_.findByIdForMember(ingredientId, memberId);
        if (!existing.has_value())
        {
            result.statusCode = 404;
            result.message = "Ingredient not found.";
            return result;
        }

        // ?섏젙 媛?ν븳 ?꾨뱶留?蹂꾨룄 DTO???대뒗??
        // 怨듦났 API?먯꽌 ?좊옒??肄붾뱶/?곸뼇 ?먮낯 ?뺣낫???ш린????뼱?곗? ?딅뒗??
        UpdateIngredientRequestDTO normalized;
        normalized.name = name;
        normalized.category = category;
        normalized.quantity = quantity;
        normalized.unit = unit;
        normalized.storage = toDbStorage(storage);
        normalized.expiryDate = expiryDate;

        if (!mapper_.updateEditableFields(ingredientId, memberId, normalized))
        {
            result.statusCode = 500;
            result.message = "Failed to update ingredient.";
            return result;
        }

        // update 寃곌낵留뚯쑝濡쒕뒗 DB ?몃━嫄곕굹 湲곕낯媛?諛섏쁺 ?щ?瑜??뚭린 ?대졄??
        // ?묐떟???ㅼ젣 ??λ맂 理쒖떊 媛믪쓣 ?닿린 ?꾪빐 ?ㅼ떆 議고쉶?쒕떎.
        auto updated = mapper_.findByIdForMember(ingredientId, memberId);
        if (!updated.has_value())
        {
            result.statusCode = 500;
            result.message = "Failed to load updated ingredient.";
            return result;
        }

        normalizeIngredientForClient(*updated);

        result.ok = true;
        result.statusCode = 200;
        result.message = "Ingredient updated.";
        result.ingredient = updated;
        return result;
    }
    catch (const std::exception &)
    {
        result.statusCode = 500;
        result.message = "Server error while updating ingredient.";
        return result;
    }
}

IngredientDeleteResultDTO IngredientService::deleteIngredient(
    std::uint64_t memberId,
    std::uint64_t ingredientId)
{
    IngredientDeleteResultDTO result;

    // ?몄뀡???녿뒗 ?붿껌? ?몄쬆?섏? ?딆? ?붿껌?쇰줈 泥섎━?쒕떎.
    if (memberId == 0)
    {
        result.statusCode = 401;
        result.message = "Unauthorized.";
        return result;
    }

    // ???id???꾩닔??
    if (ingredientId == 0)
    {
        result.statusCode = 400;
        result.message = "Ingredient id is required.";
        return result;
    }

    try
    {
        // ????앹옱猷뚮뒗 ?꾩옱 ?뚯썝???뚯쑀?ъ빞 ?쒕떎.
        // ??젣 ??떆 ?뚯쑀沅뚯쓣 癒쇱? ?뺤씤??id 異붿륫 怨듦꺽??留됰뒗??
        const auto existing = mapper_.findByIdForMember(ingredientId, memberId);
        if (!existing.has_value())
        {
            result.statusCode = 404;
            result.message = "Ingredient not found.";
            return result;
        }

        // ?ㅼ젣 row瑜?吏?곗? ?딄퀬 ??젣 ?곹깭留??쒖떆?쒕떎.
        // 異뷀썑 蹂듦뎄, 媛먯궗 濡쒓렇, ?듦퀎 怨꾩궛???먮낯 ?곗씠?곕? ?④만 ???덈떎.
        if (!mapper_.markDeleted(ingredientId, memberId))
        {
            result.statusCode = 500;
            result.message = "Failed to delete ingredient.";
            return result;
        }

        result.ok = true;
        result.statusCode = 200;
        result.message = "Ingredient deleted.";
        return result;
    }
    catch (const std::exception &)
    {
        result.statusCode = 500;
        result.message = "Server error while deleting ingredient.";
        return result;
    }
}

std::string IngredientService::trim(std::string value)
{
    // 臾몄옄???욌뮘 怨듬갚???쒓굅?섎뒗 怨듭슜 ?좏떥由ы떚??
    // ?꾩닔媛?寃利??꾩뿉 ?몄텧?댁꽌 "   " 媛숈? ?낅젰??鍮?媛믪쑝濡??먮떒?섍쾶 ?쒕떎.
    const auto notSpace = [](unsigned char ch) { return !std::isspace(ch); };

    value.erase(value.begin(),
                std::find_if(value.begin(), value.end(), notSpace));
    value.erase(
        std::find_if(value.rbegin(), value.rend(), notSpace).base(), value.end());
    return value;
}

std::string IngredientService::toLower(std::string value)
{
    // ??뚮Ц??援щ텇 ?녿뒗 鍮꾧탳瑜??꾪빐 ?뺢퇋?뷀븳??
    // storage, importYn 媛숈? enum??臾몄옄??鍮꾧탳?먯꽌 ?ъ슜?쒕떎.
    std::transform(value.begin(), value.end(), value.begin(), [](unsigned char ch) {
        return static_cast<char>(std::tolower(ch));
    });
    return value;
}

bool IngredientService::isValidDate(const std::string &value)
{
    // API ?낅젰 ?좎쭨 ?뺤떇???꾧꺽?섍쾶 ?뺤씤?쒕떎.
    // ?ш린?쒕뒗 "2024-05-12" 媛숈? 臾몄옄??紐⑥뼇留??뺤씤?섍퀬 ?ㅼ젣 議댁옱 ?좎쭨 寃利앹? ?섏? ?딅뒗??
    static const std::regex pattern("^[0-9]{4}-[0-9]{2}-[0-9]{2}$");
    return std::regex_match(value, pattern);
}

bool IngredientService::isAllowedClientStorage(const std::string &value)
{
    // ?대씪?댁뼵?몄뿉??吏?먰븯??蹂닿? ?꾩튂 ?닿굅??紐⑸줉?대떎.
    // ??蹂닿? ?꾩튂媛 異붽??섎㈃ ?ш린? toDbStorage/toClientStorage瑜??④퍡 媛깆떊?댁빞 ?쒕떎.
    return value == "fridge" || value == "freezer" || value == "roomtemp" ||
           value == "other";
}

std::string IngredientService::toDbStorage(const std::string &clientValue)
{
    // ?대씪?댁뼵???닿굅??媛믪쓣 DB ?닿굅??媛믪쑝濡?蹂?섑븳??
    // 而⑦듃濡ㅻ윭? ?꾨줎?몄뿏?쒕뒗 ?뚮Ц??媛믪쓣 ?곌퀬, DB???臾몄옄 enum 媛믪쓣 ?대떎.
    if (clientValue == "fridge")
    {
        return "FRIDGE";
    }
    if (clientValue == "freezer")
    {
        return "FREEZER";
    }
    if (clientValue == "roomtemp")
    {
        return "ROOM_TEMP";
    }
    return "OTHER";
}

std::string IngredientService::toClientStorage(const std::string &dbValue)
{
    // DB ?닿굅??媛믪쓣 ?대씪?댁뼵???닿굅??媛믪쑝濡??섎룎由곕떎.
    // DB?먯꽌 ?대? ?뚮Ц???뺥깭濡??대젮?ㅻ뒗 寃쎌슦???鍮꾪빐 trim/toLower瑜?癒쇱? ?곸슜?쒕떎.
    const auto normalized = toLower(trim(dbValue));
    if (normalized == "fridge")
    {
        return "fridge";
    }
    if (normalized == "freezer")
    {
        return "freezer";
    }
    if (normalized == "room_temp")
    {
        return "roomTemp";
    }
    return "other";
}

std::optional<std::string> IngredientService::normalizeImportYn(
    const std::optional<std::string> &value)
{
    // ?섏엯 ?щ? ?뚮옒洹몃뒗 Y/N留??④린?꾨줉 ?뺢퇋?뷀븳??
    // ?????녿뒗 媛믪? ?섎せ??臾몄옄?댁쓣 ??ν븯吏 ?딄퀬 媛??놁쓬?쇰줈 泥섎━?쒕떎.
    if (!value.has_value())
    {
        return std::nullopt;
    }

    const auto normalized = toLower(trim(*value));
    if (normalized == "y")
    {
        return "Y";
    }
    if (normalized == "n")
    {
        return "N";
    }
    return std::nullopt;
}

void IngredientService::normalizeIngredientForClient(IngredientDTO &ingredient)
{
    // 留ㅽ띁 寃곌낵 ?꾨뱶瑜??대씪?댁뼵??怨꾩빟??留욊쾶 ?뺢퇋?뷀븳??
    // ???⑥닔瑜?嫄곗튇 DTO留?而⑦듃濡ㅻ윭 ?묐떟?쇰줈 ?섍??꾨줉 ?좎??섎㈃ ?쒗쁽 蹂?섏씠 ?쒓납??紐⑥씤??
    ingredient.storage = toClientStorage(ingredient.storage);
    ingredient.importYn = normalizeImportYn(ingredient.importYn);
    ingredient.dataBaseDate = normalizeDataBaseDate(ingredient.dataBaseDate);
}

}  // namespace ingredient


