#include "DJFXLooperC.h"
#include "DJFXLooperCore.h"
#include <new>
struct RD_DJFX{raredata::DJFXLooperCore core;bool prepared=false;};
extern "C" {
RD_DJFX*rd_djfx_create(void){return new(std::nothrow)RD_DJFX;}
void rd_djfx_destroy(RD_DJFX*p){delete p;}
int rd_djfx_prepare(RD_DJFX*p,double rate){if(!p)return 0;try{p->core.prepare(rate);p->prepared=true;return 1;}catch(...){p->prepared=false;return 0;}}
void rd_djfx_set_controls(RD_DJFX*p,float length,float speed,int loop){if(p)p->core.setControls(length,speed,loop!=0);}
void rd_djfx_reset(RD_DJFX*p){if(p&&p->prepared)p->core.reset();}
void rd_djfx_process(RD_DJFX*p,float*l,float*r,int n){if(!p||!p->prepared||!l||n<=0)return;for(int i=0;i<n;++i){auto y=p->core.process(l[i],r?r[i]:l[i]);l[i]=y[0];if(r)r[i]=y[1];}}
}
