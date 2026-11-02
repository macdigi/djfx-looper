#ifndef DJFX_NATIVE_SCOPE
#define DJFX_NATIVE_SCOPE 0
#endif
#include "Vst2Abi.h"
#include "../../core/DJFXLooperCore.h"
#include <dlfcn.h>
#include <vector>
#include <iostream>
#include <cmath>
#include <cstring>
#include <fstream>
#include <chrono>
#include <algorithm>
#if DJFX_ON_DEVICE_CHECK
#include <sys/resource.h>
#endif
int main(int argc,char** argv){
#if DJFX_ON_DEVICE_CHECK
// Lower only this synthetic checker, never the instrument or scope helpers.
if(setpriority(PRIO_PROCESS,0,19)!=0)return 3;
#endif
if(argc!=2)return 2;auto* library=dlopen(argv[1],RTLD_NOW);if(!library){std::cerr<<dlerror();return 1;}auto factory=(AEffect*(*)(HostCallback))dlsym(library,"VSTPluginMain");if(!factory)return 1;int checked=0,failed=0;auto check=[&](bool yes){++checked;if(!yes)++failed;};
 for(double rate:{44100.,48000.,96000.})for(int block:{32,127,512}){auto* fx=factory(nullptr);check(fx&&fx->numInputs==2&&fx->numOutputs==2&&(fx->flags&256)==0);fx->dispatcher(fx,10,0,0,nullptr,(float)rate);raredata::DJFXLooperCore reference;reference.prepare(rate);
  std::vector<float> l(block),r(block),el(block),er(block);float* in[2]={l.data(),r.data()};float* out[2]={el.data(),er.data()};double maximum=0;
  for(int pass=0;pass<120;++pass){const float speed=pass<40?1.f:pass<80?.175f:.5f;const bool loop=pass>=20&&pass<90;fx->setParameter(fx,1,speed);fx->setParameter(fx,2,loop?1.f:0.f);reference.setControls(.230f,std::round(-100.f+200.f*speed),loop);
   for(int i=0;i<block;++i){l[i]=.2f*std::sin((pass*block+i)*.07f);r[i]=.16f*std::cos((pass*block+i)*.11f);}fx->processReplacing(fx,in,out,block);
   for(int i=0;i<block;++i){auto y=reference.process(l[i],r[i]);maximum=std::max({maximum,std::abs(double(el[i]-y[0])),std::abs(double(er[i]-y[1]))});}}
  check(maximum<2.e-6);void* chunk=nullptr;auto bytes=fx->dispatcher(fx,23,0,0,&chunk,0);std::vector<char> saved((char*)chunk,(char*)chunk+bytes);fx->setParameter(fx,1,.9f);check(fx->dispatcher(fx,24,0,bytes,saved.data(),0)==1&&std::abs(fx->getParameter(fx,1)-.5f)<1e-6);
  fx->setParameter(fx,3,0);for(int pass=0;pass<40;++pass)fx->processReplacing(fx,in,out,block);check(std::abs(el.back()-l.back())<1e-7);fx->dispatcher(fx,1,0,0,nullptr,0);
 }
 auto* a=factory(nullptr);auto* b=factory(nullptr);a->setParameter(a,0,.7f);check(std::abs(b->getParameter(b,0))<1e-7);
#if DJFX_PRIVATE_SCOPE || DJFX_NATIVE_SCOPE
check(a->getParameter(a,7)!=b->getParameter(b,7));
#else
check(a->getParameter(a,7)==1.f&&b->getParameter(b,7)==1.f); // No private display allocation.
#endif
a->dispatcher(a,1,0,0,nullptr,0);b->dispatcher(b,1,0,0,nullptr,0);
// Physical-Q-Link contract: a continuous Loop knob, with capture strictly
// above its midpoint. The independent core receives that decision on the very
// next audio block; there is no worker queue or UI-timer dependency.
auto* q=factory(nullptr);check(q->uniqueID==(DJFX_NATIVE_SCOPE?0x446a4e73:0x446a4c31)&&q->numParams==(DJFX_NATIVE_SCOPE?142:14)&&q->version==(DJFX_NATIVE_SCOPE?111:101));
VstParameterProperties props{};check(q->dispatcher(q,56,2,0,&props,0)==1&&props.flags==4);
#if DJFX_NATIVE_SCOPE
for(int i=14;i<142;++i){float before=q->getParameter(q,i);q->setParameter(q,i,1);check(q->getParameter(q,i)==before&&q->dispatcher(q,26,i,0,nullptr,0)==0);}
check(q->dispatcher(q,50,0x51465831,0,nullptr,0)==0);
#endif

q->dispatcher(q,10,0,0,nullptr,48000);raredata::DJFXLooperCore ref;ref.prepare(48000);
float l[128],r[128],lo[128],ro[128];float* in[]={l,r};float* out[]={lo,ro};double difference=0;
double maximumMicros=0;int sample=0;
for(int block=0;block<400;++block){
    const float position=block<100?.5f:block<200?.5001f:block<300?.4999f:.8f;
    q->setParameter(q,2,position);
    if(block%100==0){check(q->getParameter(q,2)==position);char text[16]{};q->dispatcher(q,7,2,0,text,0);check(std::strcmp(text,position>.5f?"ON":"OFF")==0);}
    q->setParameter(q,1,.175f);ref.setControls(.230f,-65.f,position>.5f);
    for(int i=0;i<128;++i,++sample){l[i]=.17f*std::sin(sample*.037f);r[i]=.11f*std::cos(sample*.061f);}
    auto start=std::chrono::steady_clock::now();q->processReplacing(q,in,out,128);
    maximumMicros=std::max(maximumMicros,std::chrono::duration<double,std::micro>(std::chrono::steady_clock::now()-start).count());
    for(int i=0;i<128;++i){auto y=ref.process(l[i],r[i]);difference=std::max({difference,std::abs(double(y[0]-lo[i])),std::abs(double(y[1]-ro[i]))});}
}
check(difference<2.e-6);
#if DJFX_PRIVATE_SCOPE
auto path=reinterpret_cast<const char*>(q->dispatcher(q,50,0x51465831,0,nullptr,0));
std::ifstream wire(path,std::ios::binary);uint32_t header[17]{};wire.read(reinterpret_cast<char*>(header),sizeof(header));
check(wire.gcount()==sizeof(header)&&header[0]==0x51465331&&header[1]==4&&header[13]==0);
#endif
// Existing v1 chunks and reserved positions survive the UI simplification.
q->setParameter(q,10,1);q->setParameter(q,11,1);void* data=nullptr;auto size=q->dispatcher(q,23,0,0,&data,0);
std::vector<char> state((char*)data,(char*)data+size);auto* restored=factory(nullptr);
check(restored->dispatcher(restored,24,0,size,state.data(),0)==1&&restored->getParameter(restored,10)==1&&restored->getParameter(restored,11)==1&&std::abs(restored->getParameter(restored,2)-.8f)<1.e-6);
restored->dispatcher(restored,1,0,0,nullptr,0);q->dispatcher(q,1,0,0,nullptr,0);
dlclose(library);std::cout<<"MPC effect checks="<<checked<<" failed="<<failed<<" next_block_error="<<difference<<" max_block_us="<<maximumMicros<<'\n';return failed?1:0;}
