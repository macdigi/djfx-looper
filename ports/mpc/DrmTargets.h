#pragma once
#if defined(__linux__)
#include "FrameGeometry.h"
#include <algorithm>
#include <array>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <dirent.h>
#include <dlfcn.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <unistd.h>

// Minimal public libdrm ABI / Linux DRM UAPI declarations. No host SDK,
// firmware structures, addresses, interposition or private renderer is used.
class DJFXDrmTargets {
    struct Resources {int countFbs;uint32_t* fbs;int countCrtcs;uint32_t* crtcs;int countConnectors;uint32_t* connectors;int countEncoders;uint32_t* encoders;uint32_t minWidth,maxWidth,minHeight,maxHeight;};
    struct Buffer {uint32_t id,width,height,pitch,bpp,depth,handle;};
    struct Crtc {uint32_t id,buffer,x,y,width,height;int valid;}; // Only prefix is read.
    struct MapDumb {uint32_t handle,pad;uint64_t offset;};
    struct GemClose {uint32_t handle,pad;};
    void* library=nullptr;int fd=-1;
    Resources*(*resources)(int)=nullptr;void(*freeResources)(Resources*)=nullptr;
    Buffer*(*getBuffer)(int,uint32_t)=nullptr;void(*freeBuffer)(Buffer*)=nullptr;
    Crtc*(*getCrtc)(int,uint32_t)=nullptr;void(*freeCrtc)(Crtc*)=nullptr;
public:
    struct Target {uint32_t id=0,handle=0;void* memory=MAP_FAILED;std::size_t bytes=0;rdscope::Surface surface;std::array<rdscope::Region,3> regions{};std::array<bool,3> cached{};};
    std::array<Target,4> targets{};
    DJFXDrmTargets(){
        library=dlopen("libdrm.so.2",RTLD_NOW|RTLD_LOCAL);if(!library)return;
        resources=reinterpret_cast<decltype(resources)>(dlsym(library,"drmModeGetResources"));freeResources=reinterpret_cast<decltype(freeResources)>(dlsym(library,"drmModeFreeResources"));
        getBuffer=reinterpret_cast<decltype(getBuffer)>(dlsym(library,"drmModeGetFB"));freeBuffer=reinterpret_cast<decltype(freeBuffer)>(dlsym(library,"drmModeFreeFB"));
        getCrtc=reinterpret_cast<decltype(getCrtc)>(dlsym(library,"drmModeGetCrtc"));freeCrtc=reinterpret_cast<decltype(freeCrtc)>(dlsym(library,"drmModeFreeCrtc"));
        if(!resources||!freeResources||!getBuffer||!freeBuffer||!getCrtc||!freeCrtc)return;
        fd=open("/dev/dri/card0",O_RDWR|O_CLOEXEC);
    }
    ~DJFXDrmTargets(){for(auto& target:targets)release(target);if(fd>=0)close(fd);if(library)dlclose(library);}
    DJFXDrmTargets(const DJFXDrmTargets&)=delete;
    bool available()const{return fd>=0;}
    void refresh(){
        if(fd<0)return;std::array<uint32_t,16> ids{};int count=0;
        auto append=[&](uint32_t id){if(id&&count<int(ids.size())&&std::find(ids.begin(),ids.begin()+count,id)==ids.begin()+count)ids[count++]=id;};
        // GETRESOURCES returns buffers owned by its file description. Query
        // the host's DRM descriptors read-only, then acquire/map our OWN GEM
        // handles on our separate fd. Never close or alter a host descriptor.
        if(auto* directory=opendir("/proc/self/fd")){
            for(auto* entry=readdir(directory);entry;entry=readdir(directory)){
                char path[96],link[128]{};std::snprintf(path,sizeof(path),"/proc/self/fd/%s",entry->d_name);auto length=readlink(path,link,sizeof(link)-1);
                if(length<=0||std::strcmp(link,"/dev/dri/card0"))continue;
                int hostFd=std::atoi(entry->d_name);auto* r=resources(hostFd);if(!r)continue;
                for(int i=0;i<r->countFbs&&i<16;++i)append(r->fbs[i]);freeResources(r);
            }closedir(directory);
        }
        // A standalone read-only audit can still discover the active scanout.
        if(auto* r=resources(fd)){for(int i=0;i<r->countCrtcs&&i<8;++i)if(auto* crtc=getCrtc(fd,r->crtcs[i])){append(crtc->buffer);freeCrtc(crtc);}freeResources(r);}
        for(auto& target:targets)if(target.id&&std::find(ids.begin(),ids.begin()+count,target.id)==ids.begin()+count)release(target);
        for(int i=0;i<count;++i){bool known=false;for(auto& target:targets)known|=target.id==ids[i];if(known)continue;
            auto found=std::find_if(targets.begin(),targets.end(),[](const Target& t){return !t.id;});if(found==targets.end())break;
            auto* b=getBuffer(fd,ids[i]);if(!b)continue;const bool shape=(b->width==800&&b->height==1280)||(b->width==1280&&b->height==800);
            if(shape&&b->bpp==32&&b->depth==24&&b->handle&&b->pitch>=b->width*4&&b->pitch<=8192){
                MapDumb map{b->handle,0,0};std::size_t bytes=std::size_t(b->pitch)*b->height;
                if(!ioctl(fd,_IOWR('d',0xb3,MapDumb),&map)){
                    void* memory=mmap(nullptr,bytes,PROT_READ|PROT_WRITE,MAP_SHARED,fd,map.offset);
                    if(memory!=MAP_FAILED){found->id=b->id;found->handle=b->handle;found->memory=memory;found->bytes=bytes;found->surface={static_cast<uint32_t*>(memory),bytes/4,int(b->width),int(b->height),int(b->pitch/4),1};}
                }
            }
            if(found->id!=b->id&&b->handle){GemClose close{b->handle,0};ioctl(fd,_IOW('d',0x09,GemClose),&close);}freeBuffer(b);
        }
    }
private:
    void release(Target& target){if(target.memory!=MAP_FAILED)munmap(target.memory,target.bytes);if(target.handle&&fd>=0){GemClose close{target.handle,0};ioctl(fd,_IOW('d',0x09,GemClose),&close);}target=Target{};}
};
#endif
