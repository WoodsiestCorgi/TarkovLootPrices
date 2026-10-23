#pragma once
#include <algorithm>
#include <cctype>
#include <cstdint>
#include <stdexcept>
#include <string>
#include <vector>
#include <nlohmann/json.hpp>

namespace tlp {
struct Item {
    std::string id;
    std::string name;
    std::string shortName;
    std::string gridImageLink;
    int width{1};
    int height{1};
    std::int64_t flea{0};
    std::int64_t average24h{0};
    std::int64_t trader{0};
    [[nodiscard]] std::int64_t fleaOrAverage() const {
        return flea > 0 ? flea : average24h;
    }
    [[nodiscard]] std::int64_t bestOffer() const {
        return std::max(fleaOrAverage(), trader);
    }
    [[nodiscard]] std::int64_t perSlot() const {
        return bestOffer() / (static_cast<std::int64_t>(std::max(1,width)) * std::max(1,height));
    }
};
inline std::int64_t number(const nlohmann::json& object, const char* key) {
    if (!object.is_object() || !object.contains(key) || !object.at(key).is_number_integer()) return 0;
    return object.at(key).get<std::int64_t>();
}
inline std::string string(const nlohmann::json& object, const char* key) {
    if (!object.is_object() || !object.contains(key) || !object.at(key).is_string()) return {};
    return object.at(key).get<std::string>();
}
inline std::vector<Item> parseCatalog(const std::string& payload) {
    const auto root = nlohmann::json::parse(payload);
    if (root.contains("errors")) throw std::runtime_error("GraphQL returned errors");
    if (!root.contains("data") || !root.at("data").is_object() ||
        !root.at("data").contains("items") || !root.at("data").at("items").is_array())
        throw std::runtime_error("Missing data.items in response");
    std::vector<Item> out;
    for (const auto& row : root.at("data").at("items")) {
        if (!row.is_object()) continue;
        Item item;
        item.id = string(row,"id");
        item.name = string(row,"name");
        item.shortName = string(row,"shortName");
        item.gridImageLink = string(row,"gridImageLink");
        item.width = std::max(1,static_cast<int>(number(row,"width")));
        item.height = std::max(1,static_cast<int>(number(row,"height")));
        item.flea = number(row,"lastLowPrice");
        item.average24h = number(row,"avg24hPrice");
        const auto offers = row.value("sellFor", nlohmann::json::array());
        if (offers.is_array()) {
            for (const auto& offer : offers) {
                if (!offer.is_object()) continue;
                std::string source = string(offer,"source");
                std::transform(source.begin(),source.end(),source.begin(),[](unsigned char c){return static_cast<char>(std::tolower(c));});
                if (source.find("flea") == std::string::npos)
                    item.trader = std::max(item.trader, number(offer,"priceRUB"));
            }
        }
        if (!item.id.empty() && !item.name.empty()) out.push_back(std::move(item));
    }
    if (out.empty()) throw std::runtime_error("Catalog is empty");
    return out;
}
} // namespace tlp
