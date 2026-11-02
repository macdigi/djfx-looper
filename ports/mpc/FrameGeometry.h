#pragma once
#include <array>
#include <cstdint>
#include <cstddef>

namespace rdscope {
inline std::array<uint32_t,4> marker(int slot,int corner){return {0x0b1518u,corner?uint32_t(0x101a19+corner):uint32_t(((8+(slot>>3))<<16)|((16+(slot&7))<<8)|22),uint32_t(0x0c171a+corner),0x0a1417u};}
struct Region{int x=0,y=0,slot=64,page=0,rotation=0;};
struct Surface {
    uint32_t* data=nullptr;std::size_t words=0;int physicalWidth=0,physicalHeight=0,stride=0,pages=0;
    int logicalWidth(int rotation)const{return rotation?physicalHeight:physicalWidth;}
    int logicalHeight(int rotation)const{return rotation?physicalWidth:physicalHeight;}
    std::size_t address(int x,int y,int page,int rotation)const{
        int px=x,py=y;if(rotation==1){px=y;py=physicalHeight-1-x;}else if(rotation==3){px=physicalWidth-1-y;py=x;}
        if(px<0||py<0||px>=physicalWidth||py>=physicalHeight||page<0||page>=pages)return words;
        return std::size_t(page*physicalHeight+py)*stride+px;
    }
    uint32_t read(int x,int y,int page,int rotation)const{auto i=address(x,y,page,rotation);return i<words?data[i]&0xffffff:0;}
    bool hasMarker(int x,int y,int slot,int corner,int page,int rotation)const{
        auto colors=marker(slot,corner);for(int k=0;k<4;++k)if(read(x+2+k*4,y+2,page,rotation)!=colors[k])return false;return true;
    }
    bool valid(const Region& r)const{return hasMarker(r.x,r.y,r.slot,0,r.page,r.rotation)&&hasMarker(r.x+1176,r.y,r.slot,1,r.page,r.rotation)&&hasMarker(r.x,r.y+240,r.slot,2,r.page,r.rotation)&&hasMarker(r.x+1176,r.y+240,r.slot,3,r.page,r.rotation);}
    bool locate(int page,int rotation,const std::array<bool,64>& active,Region& out)const{
        const int w=logicalWidth(rotation),h=logicalHeight(rotation);if(w<1192||h<244)return false;
        for(int y=0;y<=h-244;++y)for(int x=0;x<=w-1192;++x){
            if(read(x,y,page,rotation)!=0x0b1518||(x&&read(x-1,y,page,rotation)==0x0b1518)||(y&&read(x,y-1,page,rotation)==0x0b1518))continue;
            uint32_t color=read(x+6,y+2,page,rotation);int red=(color>>16)&255,green=(color>>8)&255;if(red<8||red>15||green<16||green>23||(color&255)!=22)continue;
            int slot=(red-8)*8+(green-16);if(!active[slot])continue;
            Region candidate{x,y,slot,page,rotation};if(valid(candidate)){out=candidate;return true;}
        }return false;
    }
    void blit(const Region& r,const uint32_t* pixels,int w,int h){
        // Revalidate all four boundaries immediately before writing. Never draw
        // over a different plugin, menu, project or system screen.
        if(!valid(r)||w!=1152||h!=228)return;
        for(int y=0;y<h;++y)for(int x=0;x<w;++x){auto i=address(r.x+20+x,r.y+8+y,r.page,r.rotation);if(i<words)data[i]=(data[i]&0xff000000)|(pixels[y*w+x]&0xffffff);}
    }
};
}
