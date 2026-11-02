#include "../core/DJFXLooperC.h"
#include "../core/DJFXLooperCore.h"
#include <cmath>
#include <iostream>
int main(){auto*handle=rd_djfx_create();if(!handle||!rd_djfx_prepare(handle,48000.))return 1;raredata::DJFXLooperCore reference;reference.prepare(48000.);float l[127],r[127];double maximum=0.;for(int block=0;block<800;++block){const bool loop=block>=200;const float speed=block<400?100.f:block<600?-60.f:0.f;rd_djfx_set_controls(handle,.07f,speed,loop);reference.setControls(.07f,speed,loop);float expected[127][2];for(int i=0;i<127;++i){l[i]=.2f*std::sin((block*127+i)*.071f);r[i]=.15f*std::cos((block*127+i)*.113f);auto y=reference.process(l[i],r[i]);expected[i][0]=y[0];expected[i][1]=y[1];}rd_djfx_process(handle,l,r,127);for(int i=0;i<127;++i)maximum=std::max({maximum,std::abs(double(l[i]-expected[i][0])),std::abs(double(r[i]-expected[i][1]))});}rd_djfx_destroy(handle);std::cout<<"C ABI versus C++ maximum="<<maximum<<'\n';return maximum==0.?0:1;}
