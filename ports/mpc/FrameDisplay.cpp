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
#include "DrmTargets.h"
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
        DJFXDrmTargets buffers;
        DJFXNativeScope::Frame frame;DJFXScopeCanvas canvas;unsigned long frames=0;unsigned sequence=0;int visible=64;uint32_t hash=0;
        char path[128];std::snprintf(path,sizeof(path),"/tmp/rd-djfx-native-%ld.metrics",long(getpid()));int stats=open(path,O_WRONLY|O_CREAT|O_EXCL|O_NOFOLLOW|O_CLOEXEC,0600);
        auto lastStats=std::chrono::steady_clock::now(),lastRefresh=lastStats-std::chrono::seconds(2);
        while(!stop.load()){
            auto now=std::chrono::steady_clock::now();if(now-lastRefresh>std::chrono::seconds(1)){buffers.refresh();lastRefresh=now;}
            bool found=false;int renderedSlot=64;std::unique_lock<std::mutex> lock(mutex);
            std::array<bool,64> active{};for(int i=0;i<64;++i)active[i]=scopes[i]!=nullptr;
            for(auto& target:buffers.targets)if(target.id)for(int rotation:{0,1,3}){
                auto& surface=target.surface;int index=rotation==0?0:rotation==1?1:2;auto& region=target.regions[index];
                if(target.cached[index]&&(!active[region.slot]||!surface.valid(region)))target.cached[index]=false;
                if(!target.cached[index])target.cached[index]=surface.locate(0,rotation,active,region);
                if(!target.cached[index])continue;auto* scope=scopes[region.slot];if(!scope)continue;found=true;visible=region.slot;
                if(renderedSlot!=region.slot){scope->readFrame(frame);sequence=frame.sequence;canvas.render(frame,1,scope->paletteValue(),scope->enabledValue(),1152,228);renderedSlot=region.slot;
                    hash=2166136261u;for(std::size_t p=0;p<canvas.pixels().size();p+=61)hash=(hash^canvas.pixels()[p])*16777619u;}
                if(!canvas.pixels().empty())surface.blit(region,canvas.pixels().data(),1152,228);
            }
            if(found)++frames;
            now=std::chrono::steady_clock::now();if(stats>=0&&now-lastStats>std::chrono::seconds(1)){
                int mapped=0;for(auto& target:buffers.targets)mapped+=bool(target.id);
                char text[320];int n=std::snprintf(text,sizeof(text),"{\"drawnFrames\":%lu,\"audioSequence\":%u,\"visibleSlot\":%d,\"markerVisible\":%s,\"imageHash\":%u,\"drmBuffers\":%d,\"renderer\":\"DRM-wave\",\"externalWorker\":false}\n",frames,sequence,visible,found?"true":"false",hash,mapped);lseek(stats,0,SEEK_SET);write(stats,text,n);ftruncate(stats,n);lastStats=now;}
            // Release the registry before sleeping (scope destruction never
            // waits on this timer, only on an in-progress bounded draw).
            lock.unlock();std::this_thread::sleep_for(found?std::chrono::milliseconds(33):std::chrono::milliseconds(250));
        }
        if(stats>=0){close(stats);unlink(path);}
#endif
    }
};
Displays& displays(){static Displays instance;return instance;}
}
DJFXFrameDisplay::DJFXFrameDisplay(DJFXNativeScope& scope):slot(displays().add(scope)){}
DJFXFrameDisplay::~DJFXFrameDisplay(){displays().remove(slot);}
