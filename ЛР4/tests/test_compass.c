#include "compass.h"
#include "fifo.h"
#include <assert.h>
#include <stdio.h>
#include <limits.h>
#include <string.h>
int main(void) {
    Compass c;compass_init(&c);assert(c.heading==-1&&c.scale[2]==100);
    assert(compass_heading(1,0)==0);assert(compass_heading(0,1)==9000);
    assert(compass_heading(-1,0)==18000);assert(compass_heading(0,-1)==27000);
    assert(compass_heading(0,0)==-1);assert(compass_heading(1,1)==4500);
    assert(compass_heading(INT_MIN,INT_MAX)==13500);
    int16_t raw[]={-32768,32767,1};compass_measure(&c,raw,14615);
    assert(c.field[0]==-478904);assert(c.field[1]==478889);
    assert(compass_scale(&c,0,200,100));compass_measure(&c,raw,14615);
    assert(c.field[0]==0&&c.field[1]==957779&&c.heading==9000);
    assert(!compass_scale(&c,201,100,100));assert(c.scale[0]==0);
    assert(compass_scale(&c,0,0,0));compass_measure(&c,raw,14615);assert(c.heading==-1);
    unsigned scales[3];
    assert(compass_command("PING",scales)==1);assert(compass_command("GET",scales)==2);
    assert(compass_command("SCALE 0 100 200",scales)==3);assert(scales[2]==200);
    const char *bad[]={"","PING x","SCALE -1 0 0","SCALE 201 0 0","SCALE 1 2","SCALE 1 2 3 x","SCALE 9999999999999999 0 0","SCALE +1 2 3","SCALE 1 2 3 "};
    for(unsigned i=0;i<sizeof bad/sizeof *bad;++i) assert(!compass_command(bad[i],scales));
    char status[180];compass_status(&c,status,sizeof status);assert(!strncmp(status,"DATA ",5));assert(strstr(status,"\r\n"));
    Fifo q={0};uint8_t byte;for(unsigned i=0;i<200000;++i){assert(fifo_put(&q,(uint8_t)i));assert(fifo_get(&q,&byte));assert(byte==(uint8_t)i);}
    puts("PASS compass: headings, zero field, signed extremes, scaling 0..200, invalid protocol, FIFO rollover");
}
