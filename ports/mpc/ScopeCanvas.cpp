#include "ScopeCanvas.h"
#include <complex>

namespace {constexpr float pi=3.14159265358979323846f;float clipped(float x){return std::clamp(std::isfinite(x)?x:0.f,-1.f,1.f);}}
void DJFXScopeCanvas::blend(int x,int y,uint32_t color,float alpha){
    if(x<0||y<0||x>=width||y>=height)return;alpha=std::clamp(alpha,0.f,1.f);auto& dst=image[y*width+x];uint32_t out=0;
    for(int shift:{0,8,16})out|=uint32_t(((dst>>shift)&255)*(1-alpha)+((color>>shift)&255)*alpha)<<shift;dst=out;
}
void DJFXScopeCanvas::line(float x0,float y0,float x1,float y1,uint32_t color,float alpha){
    // Distance coverage gives connected, antialiased lines at the actual panel
    // resolution; no pre-drawn waveform frames or quantized dots are used.
    const bool steep=std::abs(y1-y0)>std::abs(x1-x0);if(steep){std::swap(x0,y0);std::swap(x1,y1);}if(x0>x1){std::swap(x0,x1);std::swap(y0,y1);}
    const float dx=x1-x0,slope=dx>.0001f?(y1-y0)/dx:0;
    for(int x=int(std::floor(x0));x<=int(std::ceil(x1));++x){float y=y0+(x-x0)*slope;int base=int(std::floor(y));float fraction=y-base;
        if(steep){blend(base-1,x,color,alpha*.13f);blend(base,x,color,alpha*(1-fraction));blend(base+1,x,color,alpha*fraction);blend(base+2,x,color,alpha*.13f);}else{blend(x,base-1,color,alpha*.13f);blend(x,base,color,alpha*(1-fraction));blend(x,base+1,color,alpha*fraction);blend(x,base+2,color,alpha*.13f);}}
}
void DJFXScopeCanvas::rect(int x,int y,int w,int h,uint32_t color,float alpha){for(int j=std::max(0,y);j<std::min(height,y+h);++j)for(int i=std::max(0,x);i<std::min(width,x+w);++i)blend(i,j,color,alpha);}
uint32_t DJFXScopeCanvas::ink(int palette,float t)const{
    const uint32_t a=palette==1?0xba78ef:palette==2?0xefd8a9:0x82d9c8,b=palette==1?0x70c2e7:palette==2?0xf9f0de:0x79b8dc;
    uint32_t out=0;for(int shift:{0,8,16})out|=uint32_t(((a>>shift)&255)*(1-t)+((b>>shift)&255)*t)<<shift;return out;
}
void DJFXScopeCanvas::render(const DJFXNativeScope::Frame& frame,int mode,int palette,bool enabled,int w,int h){
    width=w;height=h;if(w<32||h<32||w>2048||h>1024){image.clear();return;}image.assign(size_t(w*h),0x091316);
    const float middle=h*.5f,span=h*.43f;
    line(0,middle,w-1,middle,0x75a898,.12f);for(int k=1;k<4;++k)line(w*k/4.f,8,w*k/4.f,h-9,0x5c8a82,.07f);
    if(!enabled)return;
    auto mono=[&](int i){i=std::clamp(i,0,DJFXNativeScope::frameSamples-1);return clipped((frame.left[i]+frame.right[i])*1.2f);};
    if(mode==0||mode==6){
        const int bars=128;float bw=float(w)/bars;
        for(int b=0;b<bars;++b){float v=0;for(int i=b*2048/bars;i<(b+1)*2048/bars;++i){auto m=mono(i);v=mode==6?v+m*m:std::max(v,std::abs(m));}
            if(mode==6)v=std::sqrt(v/(2048/bars));v=std::clamp(v,0.f,1.f);int length=int(v*(mode==0?span:h*.85f));uint32_t color=ink(palette,float(b)/(bars-1));
            rect(int(b*bw)+1,mode==0?int(middle)-length:h-8-length,std::max(1,int(bw)-2),mode==0?length*2:length,color,.85f);}
    }else if(mode==1){
        float previous=middle;for(int x=0;x<w;++x){float at=float(x)*2047/(w-1);int i=int(at);float f=at-i;float value=mono(i)*(1-f)+mono(std::min(i+1,2047))*f;float y=middle-value*span;
            if(x){auto color=ink(palette,float(x)/(w-1));line(x-1,previous,x,y,color,.9f);}previous=y;}
    }else if(mode==3||mode==5){
        const float radius=std::min(w,h)*.42f,cx=w*.5f,cy=h*.5f;
        line(cx-radius,cy,cx+radius,cy,0x8ad4c6,.15f);line(cx,cy-radius,cx,cy+radius,0x8ad4c6,.15f);
        float px=cx,py=cy;
        for(int i=0;i<2048;i+=2){float left=frame.left[i],right=frame.right[i];float a=mode==3?(left+right)*1.2f:left*2.4f,b=mode==3?(left-right)*1.2f:frame.left[std::min(i+11,2047)]*2.4f;
            float x=cx+clipped(a)*radius,y=cy-clipped(b)*radius;if(i)line(px,py,x,y,ink(palette,float(i)/2048),.4f+.5f*i/2048);px=x;py=y;}
    }else{
        std::array<std::complex<float>,2048> fft{};
        for(unsigned i=0;i<2048;++i){unsigned bits=i,reversed=0;for(int k=0;k<11;++k){reversed=(reversed<<1)|(bits&1);bits>>=1;}fft[reversed]={mono(i)*(.5f-.5f*std::cos(2*pi*i/2047)),0};}
        for(int size=2;size<=2048;size*=2){std::complex<float> step(std::cos(-2*pi/size),std::sin(-2*pi/size));for(int start=0;start<2048;start+=size){std::complex<float> phase(1,0);for(int j=0;j<size/2;++j){auto a=fft[start+j],b=fft[start+j+size/2]*phase;fft[start+j]=a+b;fft[start+j+size/2]=a-b;phase*=step;}}}
        std::array<float,128> spectrum{};const float high=std::min(18000.f,frame.sampleRate*.49f);
        for(int b=0;b<128;++b){int lo=std::clamp(int(30*std::pow(high/30,float(b)/128)*2048/frame.sampleRate),1,1023),hi=std::clamp(int(30*std::pow(high/30,float(b+1)/128)*2048/frame.sampleRate),1,1023);float magnitude=0;for(int k=lo;k<=hi;++k)magnitude=std::max(magnitude,std::abs(fft[k])*(4.f/2048));spectrum[b]=std::clamp((20*std::log10(std::max(1.e-6f,magnitude))+72)/72,0.f,1.f);}
        if(lastSequence!=frame.sequence){for(int b=0;b<128;++b)smoothed[b]=std::max(spectrum[b],smoothed[b]*.88f);for(int r=7;r>0;--r)history[r]=history[r-1];history[0]=smoothed;lastSequence=frame.sequence;}
        int rows=mode==4?8:1;
        for(int row=rows-1;row>=0;--row){float bottom=mode==4?h-8-row*h*.085f:h-8,amplitude=mode==4?h*.35f:h*.86f;float px=0,py=bottom-history[row][0]*amplitude;
            for(int b=1;b<128;++b){float x=float(b)*(w-1)/127,y=bottom-(mode==4?history[row][b]:smoothed[b])*amplitude;uint32_t color=ink(palette,mode==4?float(row)/8:float(b)/127);line(px,py,x,y,color,mode==4?.3f+.7f*(8-row)/8:1.f);if(mode==2)rect(int(px),int(y),std::max(1,int(x-px)),int(bottom-y),color,.06f);px=x;py=y;}}
    }
}
