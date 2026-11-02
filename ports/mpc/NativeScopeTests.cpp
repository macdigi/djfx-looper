#include "NativeScope.h"
#include <cstdlib>
#include <iostream>
#include <limits>
#include <new>
#include <thread>

static std::atomic<int> allocations{0};
void* operator new(std::size_t n) { ++allocations; if(auto* p=std::malloc(n?n:1))return p; throw std::bad_alloc(); }
void* operator new[](std::size_t n) { return ::operator new(n); }
void operator delete(void* p) noexcept { std::free(p); }
void operator delete[](void* p) noexcept { std::free(p); }
void operator delete(void* p,std::size_t) noexcept { std::free(p); }
void operator delete[](void* p,std::size_t) noexcept { std::free(p); }

int main() {
    int checks=0,failed=0;
    auto check=[&](bool value,const char* label){++checks;if(!value){++failed;std::cerr<<label<<'\n';}};
    DJFXNativeScope scope;
    std::array<float,128> l{},r{};float* audio[]={l.data(),r.data()};
    const double pi=3.14159265358979323846;
    for(double rate:{44100.,48000.,96000.}) {
        scope.setRate(rate);scope.setVisuals(false,0,0,0);
        for(int mode=0;mode<7;++mode) {
            scope.clear();scope.setMode(mode);scope.setEnabled(true);
            int sample=0;const int before=allocations.load();
            for(int block=0;block<90;++block) {
                for(int i=0;i<128;++i,++sample) {l[i]=.5f*std::sin(2*pi*1000*sample/rate);r[i]=l[i];}
                scope.push(audio,128);
            }
            check(allocations.load()==before,"Scope allocated in processing");
            float minimum=1,maximum=0;
            for(int i=0;i<64;++i) {auto v=scope.column(i);minimum=std::min(minimum,v);maximum=std::max(maximum,v);check(std::isfinite(v)&&v>=0&&v<=1,"Invalid display range");}
            check(maximum>0,"No live data");
            if(mode==2) {
                int peak=0;for(int i=1;i<64;++i)if(scope.column(i)>scope.column(peak))peak=i;
                const double high=std::min(18000.,rate*.49);
                const double frequency=40*std::pow(high/40.,(peak+.5)/64.);
                check(frequency>700&&frequency<1400,"Spectrum peak does not follow input tone");
            }
            if(mode==3) {
                for(int i=0;i<64;++i){int cell=int(std::lround(scope.column(i)*128))-1+128*int(scope.bank(i));check(cell/16==8,"Mono vector should have zero side component");}
            }
            if(mode==1)check(maximum-minimum>.1f,"Wave plot did not track bipolar audio");
            scope.setEnabled(false);for(int b=0;b<40;++b)scope.push(audio,128);
            for(int i=0;i<64;++i)check(scope.column(i)==0,"Disabled scope must select transparent frames");
        }
    }
    for(int palette=0;palette<3;++palette) {
        scope.setMode(0);scope.setEnabled(true);scope.setVisuals(false,0,palette,6);l.fill(.1f);r.fill(.1f);
        for(int b=0;b<40;++b)scope.push(audio,128);
        int frame=int(std::lround(scope.column(0)*96));check((frame-1)/32==palette,"Palette frame bank mismatch");
    }
    scope.setMode(2);scope.setEnabled(true);l.fill(std::numeric_limits<float>::quiet_NaN());r.fill(std::numeric_limits<float>::infinity());
    for(int b=0;b<40;++b)scope.push(audio,128);
    for(int i=0;i<64;++i)check(std::isfinite(scope.column(i)),"Nonfinite input escaped analysis");
    check(scope.column(-1)==0&&scope.column(64)==0,"Out-of-range display access");
    // Concurrent UI polling reads only lock-free atomics, never the audio ring.
    std::atomic<bool> stop{false};std::atomic<bool> bad{false};
    std::thread reader([&]{while(!stop.load())for(int i=0;i<64;++i){auto v=scope.column(i);if(!std::isfinite(v)||v<0||v>1)bad=true;}});
    l.fill(.2f);r.fill(-.2f);for(int b=0;b<400;++b)scope.push(audio,128);
    stop=true;reader.join();check(!bad.load(),"Concurrent UI polling failed");
    std::cout<<"Native scope checks="<<checks<<" failed="<<failed<<'\n';return failed?1:0;
}
