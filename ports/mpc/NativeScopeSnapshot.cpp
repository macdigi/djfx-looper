#include "NativeScope.h"
#include <iostream>

int main() {
    DJFXNativeScope scope;scope.setRate(48000);scope.setVisuals(false,0,0,6);
    float l[128],r[128];float* audio[]={l,r};
    std::cout<<"{\"syntheticAudio\":true,\"sampleRate\":48000,\"modes\":[";
    for(int mode=0;mode<7;++mode) {
        scope.clear();scope.setMode(mode);int sample=0;
        for(int block=0;block<300;++block) {
            for(int i=0;i<128;++i,++sample) {
                const double t=sample/48000.;
                const float envelope=.3f+.2f*std::sin(sample*.0031f);
                l[i]=envelope*(.6f*std::sin(t*6.28318530718*123)+.25f*std::sin(t*6.28318530718*738)+.1f*std::sin(t*6.28318530718*2600));
                r[i]=envelope*(.6f*std::sin(t*6.28318530718*123+.7)+.25f*std::sin(t*6.28318530718*738+.3));
            }
            scope.push(audio,128);
        }
        if(mode)std::cout<<',';std::cout<<'[';
        for(int i=0;i<64;++i){if(i)std::cout<<',';std::cout<<scope.column(i);}std::cout<<',';
        for(int i=0;i<64;++i){if(i)std::cout<<',';std::cout<<scope.bank(i);}std::cout<<']';
    }
    std::cout<<"]}\n";
}
