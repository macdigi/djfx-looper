#pragma once
#include <cstdint>
// Minimal binary interface declarations. No proprietary SDK is included.
struct AEffect;
using HostCallback = intptr_t(*)(AEffect*,int32_t,int32_t,intptr_t,void*,float);
struct AEffect {
    int32_t magic;
    intptr_t (*dispatcher)(AEffect*,int32_t,int32_t,intptr_t,void*,float);
    void (*process)(AEffect*,float**,float**,int32_t);
    void (*setParameter)(AEffect*,int32_t,float);
    float (*getParameter)(AEffect*,int32_t);
    int32_t numPrograms,numParams,numInputs,numOutputs,flags;
    intptr_t reserved1,reserved2;
    int32_t initialDelay,realQualities,offQualities;
    float ioRatio;
    void* object; void* user;
    int32_t uniqueID,version;
    void (*processReplacing)(AEffect*,float**,float**,int32_t);
    void (*processDoubleReplacing)(AEffect*,double**,double**,int32_t);
    char future[56];
};
struct VstEvent { int32_t type,byteSize,deltaFrames,flags; char data[16]; };
struct VstMidiEvent {
    int32_t type,byteSize,deltaFrames,flags,noteLength,noteOffset;
    char midiData[4],detune,noteOffVelocity,reserved1,reserved2;
};
struct VstEvents { int32_t numEvents; intptr_t reserved; VstEvent* events[2]; };
struct VstTimeInfo {
    double samplePos,sampleRate,nanoSeconds,ppqPos,tempo,barStartPos,cycleStartPos,cycleEndPos;
    int32_t timeSigNumerator,timeSigDenominator,smpteOffset,smpteFrameRate,samplesToNextClock,flags;
};
struct VstParameterProperties {
    float stepFloat,smallStepFloat,largeStepFloat;
    char label[64];
    int32_t flags,minInteger,maxInteger,stepInteger,largeStepInteger;
    char shortLabel[8];
    int16_t displayIndex,category,numParametersInCategory,reserved;
    char categoryLabel[24],future[16];
};
static_assert(sizeof(VstParameterProperties)==152);
