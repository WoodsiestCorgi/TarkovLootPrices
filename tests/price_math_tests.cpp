#include "price_math.hpp"
#include <iostream>
int main() {
    using tlp::PriceQuote;
    const PriceQuote example{850000, 125000, 50000, 2, 1};
    if (tlp::netFleaPrice(example) != 800000) return 1;
    if (tlp::bestSellPrice(example) != 800000) return 2;
    if (tlp::bestRoublesPerSlot(example) != 400000) return 3;
    if (tlp::bestRoublesPerSlot(PriceQuote{0, 9000, 0, 0, 0}) != 9000) return 4;
    if (tlp::netFleaPrice(PriceQuote{1000, 0, 2500, 1, 1}) != 0) return 5;
    std::cout << "price math passed\n";
    return 0;
}
