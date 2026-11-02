#pragma once
#include "NativeScope.h"

// Only this public implementation is used by the native source edition. It
// never calls or installs the proprietary QuotaScope worker.
class DJFXFrameDisplay {
public:
    explicit DJFXFrameDisplay(DJFXNativeScope&);
    ~DJFXFrameDisplay();
    int identity()const noexcept{return slot;}
    DJFXFrameDisplay(const DJFXFrameDisplay&)=delete;
private:int slot=64;
};
