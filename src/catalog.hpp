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
inline std::string resolveName(const nlohmann::json& row,const nlohmann::json& translations,const char* field) {
    auto key=string(row,field);
    if(translations.is_object()&&translations.contains(key)&&translations.at(key).is_string()) {
        auto translated=translations.at(key).get<std::string>();
        if(!translated.empty())return translated;
    }
    // JSON API base response uses placeholder names; fall back to a readable slug, never show the ID placeholder.
    auto slug=string(row,"normalizedName");
    std::replace(slug.begin(),slug.end(),'-',' ');
    if(!slug.empty())return slug;
    return string(row,"id");
}
inline std::vector<Item> parseCatalog(const std::string& payload,const std::string& localePayload = {}) {
    const auto root=nlohmann::json::parse(payload);
    if(!root.is_object()||!root.contains("data")||!root.at("data").is_object()||
       !root.at("data").contains("items"))
        throw std::runtime_error("Missing data.items in response");
    const auto& items=root.at("data").at("items");
    if(!items.is_array()&&!items.is_object())
        throw std::runtime_error("Unsupported items response");
    nlohmann::json translations=nlohmann::json::object();
    if(!localePayload.empty()) {
        try{
            const auto locale=nlohmann::json::parse(localePayload);
            if(locale.is_object()&&locale.contains("data")&&locale.at("data").is_object())
                translations=locale.at("data");
        }catch(const std::exception&){}
    }
    std::vector<Item> out;
    out.reserve(items.size());
    const auto add=[&](const nlohmann::json& row) {
        if(!row.is_object())return;
        Item item;
        item.id=string(row,"id");
        item.name=resolveName(row,translations,"name");
        item.shortName=resolveName(row,translations,"shortName");
        item.gridImageLink=string(row,"gridImageLink");
        item.width=std::max(1,static_cast<int>(number(row,"width")));
        item.height=std::max(1,static_cast<int>(number(row,"height")));
        item.flea=number(row,"lastLowPrice");
        item.average24h=number(row,"avg24hPrice");
        // Flat JSON API uses sellToTrader (not GraphQL's sellFor).
        for(const auto* field:{"sellToTrader","sellFor"}) {
            if(!row.contains(field)||!row.at(field).is_array())continue;
            for(const auto& offer:row.at(field)) {
                if(!offer.is_object())continue;
                const auto source=string(offer,"source");
                if(source=="fleaMarket"||source=="Flea Market")continue;
                item.trader=std::max(item.trader,number(offer,"priceRUB"));
            }
        }
        if(!item.id.empty()&&!item.name.empty())out.push_back(std::move(item));
    };
    if(items.is_array())for(const auto& row:items)add(row);
    else for(const auto& [id,row]:items.items()) { (void)id;add(row); }
    if(out.empty())throw std::runtime_error("Catalog is empty");
    return out;
}
} // namespace tlp
