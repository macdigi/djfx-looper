#include "FrameDisplay.h"
#include "FrameGeometry.h"
#include "ScopeCanvas.h"
#include <array>
#include <atomic>
#include <chrono>
#include <mutex>
#include <thread>
#include <cstdio>
#include <cstring>
#if defined(__linux__)
#include <linux/fb.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <sys/resource.h>
#include <sys/syscall.h>
#include <fcntl.h>
#include <unistd.h>
#endif

namespace {
class Displays {
public:
    std::mutex mutex;std::array<DJFXNativeScope*,64> scopes{};
    std::atomic<bool> stop{false};std::thread thread;
    ~Displays(){stop=true;if(thread.joinable())thread.join();}
    int add(DJFXNativeScope& scope){std::lock_guard<std::mutex> lock(mutex);int slot=64;for(int i=0;i<64;++i)if(!scopes[i]){slot=i;scopes[i]=&scope;break;}
        if(slot<64&&!thread.joinable()&&isInstrument())thread=std::thread([this]{run();});return slot;}
    void remove(int slot){std::lock_guard<std::mutex> lock(mutex);if(slot>=0&&slot<64)scopes[slot]=nullptr;}
private:
    bool isInstrument(){
#if defined(__linux__)
        char exe[512]{},model[128]{};auto n=readlink("/proc/self/exe",exe,sizeof(exe)-1);if(n<=0)return false;exe[n]=0;const char* base=std::strrchr(exe,'/');if(std::strcmp(base?base+1:exe,"MPC"))return false;
        FILE* file=std::fopen("/sys/firmware/devicetree/base/model","rb");if(!file)return false;std::fread(model,1,sizeof(model)-1,file);std::fclose(file);return std::strstr(model,"Akai Professional Force")!=nullptr;
#else
        return false;
#endif
    }
    void run(){
#if defined(__linux__)
        // Linux nice values are per-thread. Do not change the instrument's
        // audio threads, scheduler, network, startup or firmware settings.
        setpriority(PRIO_PROCESS,static_cast<id_t>(syscall(SYS_gettid)),15);
        int fd=open("/dev/fb0",O_RDWR|O_CLOEXEC);if(fd<0)return;fb_fix_screeninfo fixed{};fb_var_screeninfo variable{};
        if(ioctl(fd,FBIOGET_FSCREENINFO,&fixed)||ioctl(fd,FBIOGET_VSCREENINFO,&variable)||variable.bits_per_pixel!=32||variable.red.offset!=16||variable.green.offset!=8||variable.blue.offset!=0){close(fd);return;}
        void* memory=mmap(nullptr,fixed.smem_len,PROT_READ|PROT_WRITE,MAP_SHARED,fd,0);if(memory==MAP_FAILED){close(fd);return;}
        rdscope::Surface surface{static_cast<uint32_t*>(memory),fixed.smem_len/4,int(variable.xres),int(variable.yres),int(fixed.line_length/4),int(variable.yres_virtual/variable.yres)};
        surface.pages=std::min(surface.pages,3);std::array<rdscope::Region,9> regions{};std::array<bool,9> cached{};
        DJFXNativeScope::Frame frame;DJFXScopeCanvas canvas;unsigned long frames=0;unsigned sequence=0;int visible=64;uint32_t hash=0;
        char path[128];std::snprintf(path,sizeof(path),"/tmp/rd-djfx-native-%ld.metrics",long(getpid()));int stats=open(path,O_WRONLY|O_CREAT|O_EXCL|O_NOFOLLOW|O_CLOEXEC,0600);
        auto lastStats=std::chrono::steady_clock::now();
        while(!stop.load()){
            bool found=false;int renderedSlot=64;std::unique_lock<std::mutex> lock(mutex);
            std::array<bool,64> active{};for(int i=0;i<64;++i)active[i]=scopes[i]!=nullptr;
            for(int page=0;page<surface.pages;++page)for(int rotation:{0,1,3}){
                int index=page*3+(rotation==0?0:rotation==1?1:2);auto& region=regions[index];
                if(cached[index]&&(!active[region.slot]||!surface.valid(region)))cached[index]=false;
                if(!cached[index])cached[index]=surface.locate(page,rotation,active,region);
                if(!cached[index])continue;auto* scope=scopes[region.slot];if(!scope)continue;found=true;visible=region.slot;
                if(renderedSlot!=region.slot){scope->readFrame(frame);sequence=frame.sequence;canvas.render(frame,scope->modeValue(),scope->paletteValue(),scope->enabledValue(),1152,228);renderedSlot=region.slot;
                    hash=2166136261u;for(std::size_t p=0;p<canvas.pixels().size();p+=61)hash=(hash^canvas.pixels()[p])*16777619u;}
                if(!canvas.pixels().empty())surface.blit(region,canvas.pixels().data(),1152,228);
            }
            if(found)++frames;
            auto now=std::chrono::steady_clock::now();if(stats>=0&&now-lastStats>std::chrono::seconds(1)){
                char text[256];int n=std::snprintf(text,sizeof(text),"{\"drawnFrames\":%lu,\"audioSequence\":%u,\"visibleSlot\":%d,\"markerVisible\":%s,\"imageHash\":%u,\"externalWorker\":false}\n",frames,sequence,visible,found?"true":"false",hash);lseek(stats,0,SEEK_SET);write(stats,text,n);ftruncate(stats,n);lastStats=now;}
            // Release the registry before sleeping (scope destruction never
            // waits on this timer, only on an in-progress bounded draw).
            lock.unlock();std::this_thread::sleep_for(found?std::chrono::milliseconds(33):std::chrono::milliseconds(250));
        }
        if(stats>=0){close(stats);unlink(path);}munmap(memory,fixed.smem_len);close(fd);
#endif
    }
};
Displays& displays(){static Displays instance;return instance;}
}
DJFXFrameDisplay::DJFXFrameDisplay(DJFXNativeScope& scope):slot(displays().add(scope)){}
DJFXFrameDisplay::~DJFXFrameDisplay(){displays().remove(slot);}
