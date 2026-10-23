#include "catalog.hpp"
#include <cassert>
#include <iostream>
int main() {
    const auto items = tlp::parseCatalog(R"({"data":{"items":[
     {"id":"gpu","name":"Graphics card","shortName":"GPU","width":2,"height":1,
      "lastLowPrice":850000,"avg24hPrice":800000,"gridImageLink":"https://example.com/gpu.png",
      "sellFor":[{"source":"fleaMarket","priceRUB":850000},{"source":"therapist","priceRUB":125000}]},
     {"id":"bolts","name":"Bolts","width":1,"height":1,
      "lastLowPrice":null,"avg24hPrice":42000,"sellFor":[]}
    ]}})");
    assert(items.size()==2);
    assert(items[0].id=="gpu");
    assert(items[0].trader==125000);
    assert(items[0].perSlot()==425000);
    assert(items[1].fleaOrAverage()==42000);
    assert(items[1].perSlot()==42000);
    bool rejected = false;
    try { static_cast<void>(tlp::parseCatalog(R"({"errors":[{"message":"fail"}]})")); }
    catch (const std::exception&) { rejected=true; }
    assert(rejected);
    std::cout << "catalog tests passed\n";
    return 0;
}
