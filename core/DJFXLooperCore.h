#pragma once
#ifndef RAREDATA_DJFX_USE_EXISTING_KERNEL
#include "DJFXKernel.h"
#endif
#include <cstdint>

namespace raredata {
// Portable accepted QuotaFX adapter, separate from JUCE and the private scopes.
class DJFXLooperCore {
public:
    void prepare(double sr){rate=std::isfinite(sr)&&sr>=8000.?std::min(sr,768000.):48000.;kernel.prepare(rate);activeAlpha=static_cast<float>(1.-std::exp(-1./(.005*rate)));speedAlpha=static_cast<float>(1.-std::exp(-1./(.001*rate)));resetPlayback();}
    void setControls(float seconds,float percent,bool loop)noexcept{length=seconds;speed=std::isfinite(percent)?std::clamp(percent*.01,-1.,1.):1.;capturing=loop;kernel.setParameters(length,speed);}
    void reset()noexcept{kernel.reset();resetPlayback();}
    void resetPlayback()noexcept{kernel.setParameters(length,speed);kernel.resetPlayback();active=0.f;remaining=static_cast<int>(std::ceil(.060*rate));speedGate=speed==0.?0.f:1.f;}
    void feed(float l,float r)noexcept{kernel.setParameters(length,1.);(void)kernel.process(l,r,0.);}
    std::array<float,2> process(float left,float right)noexcept{
        const float l=std::isfinite(left)?left:0.f,r=std::isfinite(right)?right:0.f;
        if(speed==0.)speedGate=0.f;else speedGate+=speedAlpha*(1.f-speedGate);
        const auto wet=kernel.process(l,r,capturing?1.:0.,speed==0.?0.f:speedGate);
        active+=activeAlpha*(1.f-active);if(remaining>0)--remaining;
        if(remaining==0||active>.99999f){active=1.f;remaining=0;}
        constexpr float headroom=.8413951416f;
        return {l+active*(static_cast<float>(wet[0])*headroom-l),r+active*(static_cast<float>(wet[1])*headroom-r)};
    }
private:qfx::DJFXKernel kernel;double rate=48000.,speed=1.;float length=.230f,active=0.f,speedGate=1.f,activeAlpha=1.f,speedAlpha=1.f;int remaining=0;bool capturing=false;
};
}
