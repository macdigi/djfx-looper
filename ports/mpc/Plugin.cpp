#ifndef DJFX_NATIVE_SCOPE
#define DJFX_NATIVE_SCOPE 0
#endif
#include "Vst2Abi.h"
#include "ScopeBridge.h"
#include "../../core/DJFXLooperCore.h"
#include <atomic>
#include <cmath>
#include <cstring>
#include <cstdio>
#include <memory>
namespace {
constexpr int stateCount=14;
constexpr int count=stateCount+(DJFX_NATIVE_SCOPE?128:0);constexpr int uid=DJFX_NATIVE_SCOPE?0x446a4e73:0x446a4c31;
constexpr int vendorVersion=DJFX_NATIVE_SCOPE?110:101;
const char* scopeNames[]={"PEAKS","WAVE","SPECTRUM","VECTOR","DEPTH","TRACE","BARS"};
const char* paletteNames[]={DJFX_NATIVE_SCOPE?"MINT":"GRADIENT",DJFX_NATIVE_SCOPE?"VIOLET":"RAINBOW",DJFX_NATIVE_SCOPE?"CREAM":"FLAT"};
bool engaged(float value){return value>.5f;}
const char* names[stateCount]={"Length","Speed","Loop","FX On","Scope","ScopeOn","Expand","Display","Palette","Surface","Sliders","LoopView","Momentary","LoopHold"};
struct State{uint32_t magic=0x444a4658,version=1;float values[stateCount]{};};
float norm(float x){return std::isfinite(x)?std::clamp(x,0.f,1.f):0.f;}
struct Plugin {
    AEffect fx{};raredata::DJFXLooperCore core;DJFXScopeBridge scope;std::atomic<float> p[count];State chunk;double rate=44100;float gain=1;bool previousOn=true;
    Plugin(){for(auto& v:p)v.store(0);p[1]=1;p[3]=1;p[5]=1;core.prepare(rate);scope.setRate(rate);}
    void set(int i,float v){if(i<0||i>=stateCount||i==7)return;p[i].store(norm(v));if(i==12&&v>=.5f)p[2].store(0);}
    float get(int i)const{
#if DJFX_NATIVE_SCOPE
        if(i>=stateCount&&i<stateCount+64)return scope.column(i-stateCount);
        if(i>=stateCount+64&&i<count)return scope.bank(i-stateCount-64);
#endif
        return i==7?scope.displayIdentity()/64.f:i>=0&&i<count?p[i].load():0;}
    void process(float** in,float** out,int n,bool add){
        if(!out||!out[0]||!out[1]||n<=0)return;
        const float length=std::exp(std::log(.230f)+p[0].load()*(std::log(.012f)-std::log(.230f)));
        const float speed=std::round(-100.f+200.f*p[1].load());const bool on=engaged(p[3].load());
        core.setControls(length,speed,engaged(p[2].load())||engaged(p[13].load()));if(on&&!previousOn)core.resetPlayback();previousOn=on;
        const float step=1.f/(float)(rate*.005);
        for(int i=0;i<n;++i){float l=in&&in[0]?in[0][i]:0,r=in&&in[1]?in[1][i]:l;std::array<float,2> wet{l,r};
            if(on||gain>0)wet=core.process(l,r);else core.feed(l,r);gain=std::clamp(gain+(on?step:-step),0.f,1.f);
            float a=l+gain*(wet[0]-l),b=r+gain*(wet[1]-r);if(add){out[0][i]+=a;out[1][i]+=b;}else{out[0][i]=a;out[1][i]=b;}}
        // The skin already draws its own steel colours. Theme zero avoids the
        // shared overlay's extra whole-screen recolouring pass for this instance.
        scope.setMode((int)std::round(p[4].load()*6));scope.setEnabled(p[5].load()>=.5f);scope.setVisuals(engaged(p[6].load()),0,(int)std::round(p[8].load()*2),6);scope.push(out,n);
    }
};
Plugin& self(AEffect* e){return *static_cast<Plugin*>(e->object);}
intptr_t dispatch(AEffect* e,int32_t op,int32_t index,intptr_t value,void* data,float option){auto& p=self(e);switch(op){
case 0:return 1;case 1:delete &p;return 1;
case 10:if(!std::isfinite(option)||option<8000||option>192000)return 0;p.rate=option;p.core.prepare(option);p.scope.setRate(option);return 1;
case 11:return 1;case 12:if(value){p.core.reset();p.gain=engaged(p.p[3].load())?1.f:0.f;}return 1;
case 6:if(data)std::snprintf((char*)data,8,"%s",index==0?"s":index==1?"%":"");return 1;
case 7:if(!data||index<0||index>=count)return 0;if(index>=stateCount){std::snprintf((char*)data,8,"%.3f",p.get(index));return 1;}if(index==0)std::snprintf((char*)data,8,"%.3f",std::exp(std::log(.230f)+p.get(0)*(std::log(.012f)-std::log(.230f))));else if(index==1)std::snprintf((char*)data,8,"%.0f",std::round(-100.f+200.f*p.get(1)));else if(index==2||index==3)std::snprintf((char*)data,8,"%s",engaged(p.get(index))?"ON":"OFF");else if(index==5)std::snprintf((char*)data,8,"%s",p.get(index)>=.5f?"ON":"OFF");else if(index==6)std::snprintf((char*)data,8,"%s",engaged(p.get(index))?"FULL":"PANEL");else if(index==4)std::snprintf((char*)data,8,"%s",scopeNames[(int)std::round(p.get(index)*6)]);else if(index==8)std::snprintf((char*)data,8,"%s",paletteNames[(int)std::round(p.get(index)*2)]);else std::snprintf((char*)data,8,"%.3f",p.get(index));return 1;
case 8:if(!data||index<0||index>=count)return 0;if(index>=stateCount)std::snprintf((char*)data,9,"Plot %02d",index-stateCount+1);else std::snprintf((char*)data,9,"%s",names[index]);return 1;
case 23:if(!data)return 0;for(int i=0;i<stateCount;++i)p.chunk.values[i]=p.get(i);p.chunk.values[13]=0;if(p.get(12)>=.5f)p.chunk.values[2]=0;*(void**)data=&p.chunk;return sizeof(State);
case 24:{if(!data||value!=sizeof(State))return 0;State state;std::memcpy(&state,data,sizeof(state));if(state.magic!=0x444a4658||state.version!=1)return 0;for(int i=0;i<stateCount;++i)if(!std::isfinite(state.values[i]))return 0;for(int i=0;i<stateCount;++i)p.set(i,state.values[i]);p.p[13]=0;return 1;}
case 26:return index>=0&&index<stateCount&&index!=7;case 35:return 1;
case 45:case 48:if(data)std::strcpy((char*)data,DJFX_NATIVE_SCOPE?"DJFX Looper Native":"DJFX Looper");return 1;
case 47:if(data)std::strcpy((char*)data,"Rare Data");return 1;
case 49:return vendorVersion;
case 50:
#if DJFX_PRIVATE_SCOPE
    return index==0x51465831?reinterpret_cast<intptr_t>(p.scope.path().c_str()):0;
#else
    return 0;
#endif
case 56:{if(!data||index<0||index>=stateCount||index==7)return 0;auto* props=static_cast<VstParameterProperties*>(data);std::memset(props,0,sizeof(*props));std::snprintf(props->label,sizeof(props->label),"%s",names[index]);std::snprintf(props->shortLabel,sizeof(props->shortLabel),"%s",names[index]);props->stepFloat=.005f;props->smallStepFloat=.001f;props->largeStepFloat=.01f;props->flags=4;
    // Loop remains a continuous normalized Q-Link; its ON/OFF decision is made
    // above 0.5 in the audio block, never by a two-position touchscreen widget.
    if(index==4||index==8){int maximum=index==4?6:2;props->flags=2|8;props->maxInteger=maximum;props->stepInteger=props->largeStepInteger=1;props->stepFloat=props->smallStepFloat=props->largeStepFloat=1.f/maximum;}
    if(index==5){props->flags=1;props->maxInteger=1;props->stepInteger=props->largeStepInteger=1;props->stepFloat=props->smallStepFloat=props->largeStepFloat=1.f;}
    return 1;}
case 58:return 2400;default:return 0;}}
}
extern "C" __attribute__((visibility("default"))) AEffect* VSTPluginMain(HostCallback){try{auto p=std::make_unique<Plugin>();auto& f=p->fx;f.magic=0x56737450;f.dispatcher=dispatch;f.object=p.get();f.numPrograms=1;f.numParams=count;f.numInputs=2;f.numOutputs=2;f.flags=16|32;f.uniqueID=uid;f.version=vendorVersion;f.setParameter=[](AEffect* e,int32_t i,float v){self(e).set(i,v);};f.getParameter=[](AEffect* e,int32_t i){return self(e).get(i);};f.processReplacing=[](AEffect* e,float** in,float** out,int32_t n){self(e).process(in,out,n,false);};f.process=[](AEffect* e,float** in,float** out,int32_t n){self(e).process(in,out,n,true);};return &p.release()->fx;}catch(...){return nullptr;}}
