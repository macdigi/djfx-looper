#pragma once

// The audio adapter also builds without Rare Data's private scope implementation.
#if DJFX_PRIVATE_SCOPE && !DJFX_NATIVE_SCOPE
#include "ScopeTap.h"
using DJFXScopeBridge = qfxforce::ScopeTap;
#else
struct DJFXScopeBridge {
    void setRate(double) noexcept {}
    void setMode(int) noexcept {}
    void setEnabled(bool) noexcept {}
    void setVisuals(bool,float,int,int) noexcept {}
    void push(float**,int) noexcept {}
    int displayIdentity() const noexcept { return 64; }
};
#endif
