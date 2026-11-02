#pragma once
#ifdef __cplusplus
extern "C" {
#endif
typedef struct RD_DJFX RD_DJFX;
RD_DJFX* rd_djfx_create(void);
void rd_djfx_destroy(RD_DJFX*);
int rd_djfx_prepare(RD_DJFX*,double sample_rate);
void rd_djfx_set_controls(RD_DJFX*,float length_seconds,float speed_percent,int loop_on);
void rd_djfx_reset(RD_DJFX*);
void rd_djfx_process(RD_DJFX*,float*left,float*right,int frames);
#ifdef __cplusplus
}
#endif
