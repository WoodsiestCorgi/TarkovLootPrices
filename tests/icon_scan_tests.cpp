#include "icon_scan.hpp"
#include <cassert>
#include <iostream>
int main(){
    tlp::Pattern p; p.id="test";
    for(int i=0;i<64;++i){p.alpha[i]=255;p.premul[i]=120;}
    std::vector<unsigned char> frame(64*64*4,0);
    for(int y=0;y<64;++y)for(int x=0;x<64;++x){auto* pix=&frame[(y*64+x)*4];pix[0]=pix[1]=pix[2]=120;pix[3]=255;}
    tlp::ImageView image{frame.data(),64,64,64*4};
    if(tlp::comparePattern(image,0,0,64,64,p)>0.001)return 1;
    if(tlp::scanGrid(image,{p},63,2.0).size()!=1)return 2;
    if(tlp::scanGrid(image,{p},63,2.0).front().id!="test")return 3;
    std::cout<<"icon scan passed\n";
}
