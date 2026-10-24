#include "catalog.hpp"
#include <iostream>
int main(){
    const auto items=tlp::parseCatalog(R"({"data":{"items":[
    {"id":"gpu","name":"gpu Name","shortName":"gpu ShortName","normalizedName":"graphics-card",
     "width":2,"height":1,"lastLowPrice":850000,"avg24hPrice":800000,
     "gridImageLink":"https://example.org/gpu.webp",
     "sellToTrader":[{"trader":"therapist","priceRUB":125000}]},
    {"id":"bolts","name":"bolts Name","normalizedName":"bolts",
     "width":1,"height":1,"lastLowPrice":null,"avg24hPrice":42000,"sellToTrader":[]}
    ]}})",R"({"data":{"gpu Name":"Видеокарта","gpu ShortName":"ВК"}})");
    if(items.size()!=2)return 1;
    if(items[0].id!="gpu"||items[0].name!="Видеокарта")return 2;
    if(items[0].shortName!="ВК")return 3;
    if(items[0].trader!=125000)return 4;
    if(items[0].perSlot()!=425000)return 5;
    if(items[1].name!="bolts"||items[1].fleaOrAverage()!=42000)return 6;
    if(items[1].perSlot()!=42000)return 7;
    const auto map=tlp::parseCatalog(R"({"data":{"items":{"abc":{"id":"abc","normalizedName":"small-bag","width":1,"height":1,"lastLowPrice":7000,"sellToTrader":[{"priceRUB":10000}]}}}})");
    if(map.size()!=1||map[0].trader!=10000||map[0].name!="small bag")return 8;
    bool rejected=false;
    try{static_cast<void>(tlp::parseCatalog(R"({"errors":[{"message":"fail"}]})"));}
    catch(const std::exception&){rejected=true;}
    if(!rejected)return 9;
    std::cout<<"PvE JSON catalog tests passed\n";
    return 0;
}
