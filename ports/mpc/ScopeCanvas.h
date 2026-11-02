#pragma once
#include "NativeScope.h"
#include <vector>

// Original public display compositor. No private QuotaScope implementation.
class DJFXScopeCanvas {
public:
    void render(const DJFXNativeScope::Frame&,int mode,int palette,bool enabled,int width,int height);
    const std::vector<uint32_t>& pixels() const noexcept { return image; }
private:
    int width=0,height=0;
    unsigned lastSequence=0;
    std::vector<uint32_t> image;
    std::array<std::array<float,128>,8> history{};
    std::array<float,128> smoothed{};
    void blend(int,int,uint32_t,float);
    void line(float,float,float,float,uint32_t,float=1.f);
    void rect(int,int,int,int,uint32_t,float=1.f);
    uint32_t ink(int,float) const;
};
