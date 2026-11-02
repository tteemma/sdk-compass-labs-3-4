#include "compass.h"
#include <math.h>
#include <stdio.h>
#include <string.h>
void compass_init(Compass *c) { *c=(Compass){.scale={100,100,100},.heading=-1}; }
bool compass_scale(Compass *c,unsigned x,unsigned y,unsigned z) {
    if(x>200||y>200||z>200) return false;
    c->scale[0]=(uint16_t)x; c->scale[1]=(uint16_t)y; c->scale[2]=(uint16_t)z; return true;
}
int compass_heading(int32_t x,int32_t y) {
    if(x==0 && y==0) return -1;
    float angle=atan2f((float)y,(float)x)*5729.5779513f;
    int result=(int)lroundf(angle); if(result<0) result+=36000; return result%36000;
}
void compass_measure(Compass *c,const int16_t raw[3],int sensitivity) {
    for(unsigned i=0;i<3;++i) c->field[i]=(int32_t)((int64_t)raw[i]*sensitivity*c->scale[i]/100000);
    c->heading=compass_heading(c->field[0],c->field[1]);
}
int compass_command(const char *s,unsigned scales[3]) {
    if(!strcmp(s,"PING")) return 1;
    if(!strcmp(s,"GET")) return 2;
    if(strncmp(s,"SCALE ",6)) return 0;
    s+=6;
    for(unsigned i=0;i<3;++i) {
        if(*s<'0'||*s>'9') return 0;
        unsigned value=0;
        do { value=value*10+(unsigned)(*s++-'0'); if(value>200) return 0; } while(*s>='0'&&*s<='9');
        scales[i]=value;
        if(i<2) { if(*s++!=' ') return 0; } else if(*s) return 0;
    }
    return 3;
}
void compass_status(const Compass *c,char *out,unsigned size) {
    snprintf(out,size,"DATA %lu %ld %ld %ld %d %u %u %u %u %u %u %lu %lu\r\n",
       (unsigned long)c->tick,(long)c->field[0],(long)c->field[1],(long)c->field[2],c->heading,
       c->scale[0],c->scale[1],c->scale[2],c->sensor_ok,c->display_ok,c->sensor_id,
       (unsigned long)c->frames,(unsigned long)c->frame_ms);
}
