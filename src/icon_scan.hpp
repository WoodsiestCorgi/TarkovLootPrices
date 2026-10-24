#pragma once
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <string>
#include <vector>
#include <nlohmann/json.hpp>

namespace tlp {
struct Pattern {
    std::string id;
    int width{1}, height{1};
    std::array<std::uint8_t,64> premul{}, alpha{};
};
struct Found {
    std::string id;
    int x{}, y{}, width{}, height{};
    double error{};
};
struct ImageView {
    const std::uint8_t* bgra{};
    int width{}, height{}, stride{};
};
inline std::vector<Pattern> parsePatterns(const std::string& text) {
    const auto j=nlohmann::json::parse(text);
    std::vector<Pattern> result;
    if(!j.is_object() || !j.value("version",0) || !j.contains("items") || !j.at("items").is_array()) return {};
    for(const auto& entry:j.at("items")) {
        if(!entry.is_object())continue;
        auto p=Pattern{};
        p.id=entry.value("id",std::string{});
        p.width=entry.value("width",1);p.height=entry.value("height",1);
        if(p.id.empty()||p.width<1||p.width>10||p.height<1||p.height>12)continue;
        if(!entry.contains("fg") || !entry.contains("a") || !entry["fg"].is_array() || !entry["a"].is_array() || entry["fg"].size()!=64 || entry["a"].size()!=64)continue;
        bool okay=true;
        for(int i=0;i<64;++i){
            if(!entry["fg"][i].is_number_integer()||!entry["a"][i].is_number_integer()) {okay=false;break;}
            const int fg=entry["fg"][i].get<int>(),a=entry["a"][i].get<int>();
            if(fg<0||fg>255||a<0||a>255){okay=false;break;}
            p.premul[i]=static_cast<std::uint8_t>(fg);
            p.alpha[i]=static_cast<std::uint8_t>(a);
        }
        if(okay) result.push_back(std::move(p));
    }
    return result;
}
inline double comparePattern(const ImageView& image, int x, int y, int cw, int ch, const Pattern& p) {
    if(!image.bgra || x<0 || y<0 || cw<8 || ch<8 || x+cw>image.width || y+ch>image.height)return 1e6;
    double obs[64]{}, sumBg=0, denom=0;
    int opaquePixels=0;
    for(int row=0;row<8;++row) for(int col=0;col<8;++col){
        const int i=row*8+col;
        const int sx=x+std::min(cw-1,(col*2+1)*cw/16);
        const int sy=y+std::min(ch-1,(row*2+1)*ch/16);
        const auto* pixel=image.bgra+(sy*image.stride+sx*4);
        obs[i]=0.114*pixel[0]+0.587*pixel[1]+0.299*pixel[2];
        const double transparency=(255.0-p.alpha[i])/255.0;
        sumBg+=(obs[i]-p.premul[i])*transparency;
        denom+=transparency*transparency;
        if(p.alpha[i]>100)++opaquePixels;
    }
    if(opaquePixels<5)return 1e6;
    const double background=std::clamp(sumBg/std::max(denom,0.001),0.0,255.0);
    double residual=0;
    for(int i=0;i<64;++i){
        const double expected=p.premul[i]+(255.0-p.alpha[i])*background/255.0;
        const double difference=obs[i]-expected;
        residual+=difference*difference;
    }
    return std::sqrt(residual/64.0);
}
inline std::vector<Found> scanGrid(const ImageView& image, const std::vector<Pattern>& patterns, int pitch=63, double maxError=21.0) {
    std::vector<Found> candidates;
    if(pitch<12||patterns.empty()||!image.bgra)return candidates;
    const int cols=(image.width-1)/pitch,rows=(image.height-1)/pitch;
    if(cols<1||rows<1)return candidates;
    for(int row=0;row<rows;++row) for(int col=0;col<cols;++col){
        Found best{};best.error=1e6;
        double nextError=1e6;
        for(const auto& p:patterns){
            if(col+p.width>cols||row+p.height>rows)continue;
            const double error=comparePattern(image,col*pitch,row*pitch,p.width*pitch+1,p.height*pitch+1,p);
            if(error<best.error){nextError=best.error;best={p.id,col,row,p.width,p.height,error};}
            else if(error<nextError)nextError=error;
        }
        if(best.error<maxError && (nextError-best.error>1.2 || nextError>maxError))candidates.push_back(std::move(best));
    }
    std::sort(candidates.begin(),candidates.end(),[](const Found& a,const Found& b){return a.error<b.error;});
    std::vector<std::uint8_t> used(static_cast<std::size_t>(cols*rows),0);
    std::vector<Found> selected;
    for(const auto& candidate:candidates){
        bool blocked=false;
        for(int dy=0;dy<candidate.height;++dy)for(int dx=0;dx<candidate.width;++dx)
            if(used[static_cast<std::size_t>((candidate.y+dy)*cols+candidate.x+dx)])blocked=true;
        if(blocked)continue;
        for(int dy=0;dy<candidate.height;++dy)for(int dx=0;dx<candidate.width;++dx)
            used[static_cast<std::size_t>((candidate.y+dy)*cols+candidate.x+dx)]=1;
        selected.push_back(candidate);
    }
    return selected;
}
} // namespace tlp
