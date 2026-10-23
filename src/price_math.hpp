#pragma once
#include <algorithm>
#include <cstdint>

namespace tlp {

struct PriceQuote {
    std::int64_t fleaRoubles = 0;
    std::int64_t traderRoubles = 0;
    std::int64_t fleaFeeRoubles = 0;
    int width = 1;
    int height = 1;
};

[[nodiscard]] constexpr std::int64_t netFleaPrice(const PriceQuote& q) noexcept {
    return std::max<std::int64_t>(0, q.fleaRoubles - q.fleaFeeRoubles);
}

[[nodiscard]] constexpr std::int64_t bestSellPrice(const PriceQuote& q) noexcept {
    return std::max(netFleaPrice(q), q.traderRoubles);
}

[[nodiscard]] constexpr std::int64_t bestRoublesPerSlot(const PriceQuote& q) noexcept {
    const auto slots = static_cast<std::int64_t>(std::max(1, q.width)) *
                       static_cast<std::int64_t>(std::max(1, q.height));
    return bestSellPrice(q) / slots;
}
}  // namespace tlp
