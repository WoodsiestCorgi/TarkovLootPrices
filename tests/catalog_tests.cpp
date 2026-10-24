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
    if(items.size()!=2)return 1;
    if(items[0].id!="gpu")return 2;
    if(items[0].trader!=125000)return 3;
    if(items[0].perSlot()!=425000)return 4;
    if(items[1].fleaOrAverage()!=42000)return 5;
    if(items[1].perSlot()!=42000)return 6;
    bool rejected = false;
    try { static_cast<void>(tlp::parseCatalog(R"({"errors":[{"message":"fail"}]})")); }
    catch (const std::exception&) { rejected=true; }
    if(!rejected)return 7;
    std::cout << "catalog tests passed\n";
    return 0;
}
