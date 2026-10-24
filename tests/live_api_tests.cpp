#include "network.hpp"
#include "catalog.hpp"
#include <iostream>
#include <algorithm>

int main() {
    try {
        const auto raw=tlp::fetchPvePrices();
        const auto translations=tlp::fetchPveRussian();
        const auto items=tlp::parseCatalog(raw,translations);
        const auto priced=std::count_if(items.begin(),items.end(),[](const tlp::Item& i){
            return i.fleaOrAverage()>0;
        });
        const auto names=std::count_if(items.begin(),items.end(),[](const tlp::Item& i){
            return i.name.find(" Name")==std::string::npos;
        });
        std::cout<<"PvE JSON live integration: "<<items.size()<<" items, "
                 <<priced<<" market-priced items, "
                 <<names<<" resolved names\n";
        if(items.size()<4000 || priced<100 || names<3000) {
            std::cerr<<"Incomplete market/translation data\n";
            return 1;
        }
        return 0;
    } catch(const std::exception& ex) {
        std::cerr<<"PvE JSON API smoke test failed: "<<ex.what()<<'\n';
        return 2;
    }
}
