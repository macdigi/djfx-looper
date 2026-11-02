#include "ScopeCanvas.h"
#include "FrameGeometry.h"
#include <iostream>
#include <fstream>
#include <string>

static uint32_t hash(const std::vector<uint32_t>& pixels){uint32_t v=2166136261u;for(auto p:pixels)v=(v^p)*16777619u;return v;}
int main(int argc,char** argv){
    int count=0,failed=0;auto check=[&](bool value,const char* label){++count;if(!value){++failed;std::cerr<<label<<'\n';}};
    DJFXNativeScope::Frame a,b;a.sequence=2;b.sequence=4;a.sampleRate=b.sampleRate=48000;
    for(int i=0;i<2048;++i){float t=float(i)/48000;a.left[i]=.4f*std::sin(t*6.2831853f*330);a.right[i]=.3f*std::sin(t*6.2831853f*330+.7f);b.left[i]=.12f*std::sin(t*6.2831853f*1300);b.right[i]=.2f*std::cos(t*6.2831853f*710);}
    DJFXScopeCanvas canvas;
    for(int mode=0;mode<7;++mode){canvas.render(a,mode,0,true,1152,228);auto first=hash(canvas.pixels());check(canvas.pixels().size()==1152*228,"Wrong canvas size");
        if(mode==1){int lit=0;for(auto pixel:canvas.pixels())if(((pixel>>8)&255)>110)++lit;check(lit>1000,"Wave must be a connected high-resolution trace");}
        if(argc==2){std::ofstream stream(std::string(argv[1])+"/scope-"+std::to_string(mode)+".ppm",std::ios::binary);stream<<"P6\n1152 228\n255\n";for(auto pixel:canvas.pixels()){char rgb[]={char(pixel>>16),char(pixel>>8),char(pixel)};stream.write(rgb,3);}}
        canvas.render(b,mode,0,true,1152,228);check(first!=hash(canvas.pixels()),"New audio frame did not change the rendered display");
        auto changed=hash(canvas.pixels());canvas.render(b,mode,1,true,1152,228);check(changed!=hash(canvas.pixels()),"Ink control did not affect the scope");
    }
    canvas.render(a,1,0,false,1152,228);auto off=hash(canvas.pixels());canvas.render(b,1,2,false,1152,228);check(off==hash(canvas.pixels()),"Disabled display must not show incoming audio");
    DJFXNativeScope scope;scope.setRate(48000);float l[128],r[128];float* audio[]={l,r};DJFXNativeScope::Frame received;
    for(int n=0;n<40;++n){for(int i=0;i<128;++i){l[i]=a.left[(n*128+i)%2048];r[i]=a.right[(n*128+i)%2048];}scope.push(audio,128);}
    check(scope.readFrame(received)&&received.sequence>0&&!(received.sequence&1),"No coherent lock-free audio frame");check(received.sampleRate==48000,"Frame rate metadata lost");
    // Exercise all buffer pages and both portrait rotations used by DRM-based
    // Force screens, plus a regular landscape framebuffer.
    for(int rotation:{0,1,3}){
        int pw=rotation?800:1280,ph=rotation?1280:800;std::vector<uint32_t> memory(size_t(pw*ph*3),0xff151d22);
        rdscope::Surface surface{memory.data(),memory.size(),pw,ph,pw,3};std::array<bool,64> active{};active[5]=true;
        for(int page=0;page<3;++page){for(int corner=0;corner<4;++corner){int x=40+(corner==1||corner==3?1176:0),y=224+(corner>=2?240:0);auto colors=rdscope::marker(5,corner);
            for(int k=0;k<4;++k)for(int j=0;j<4;++j)for(int i=0;i<4;++i)memory[surface.address(x+k*4+i,y+j,page,rotation)]=0xff000000|colors[k];}
            rdscope::Region region;check(surface.locate(page,rotation,active,region),"Marker not found on rotated/panned framebuffer");check(region.x==40&&region.y==224&&region.slot==5,"Incorrect drawing region");
            auto before=memory;canvas.render(a,1,0,true,1152,228);surface.blit(region,canvas.pixels().data(),1152,228);int written=0;
            for(int y=0;y<800;++y)for(int x=0;x<1280;++x){auto index=surface.address(x,y,page,rotation);if(memory[index]!=before[index]){++written;check(x>=60&&x<1212&&y>=232&&y<460,"Renderer wrote outside its own marked scope panel");}}
            check(written>1000,"No framebuffer scope pixels changed");
            memory[surface.address(1218,466,page,rotation)]^=1;auto invalid=memory;surface.blit(region,canvas.pixels().data(),1152,228);check(invalid==memory,"Renderer drew after a marker disappeared");
        }
    }
    std::cout<<"Connected scope checks="<<count<<" failed="<<failed<<'\n';return failed?1:0;
}
