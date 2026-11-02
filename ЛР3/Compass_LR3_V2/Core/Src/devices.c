#include "devices.h"
#include "board.h"
#include <string.h>
#include <stdio.h>
#include <math.h>
static I2C_HandleTypeDef bus;
static uint8_t mag_address,mag_id;
static int sensitivity;
static bool oled_ok;
static uint8_t framebuffer[1024],chunk;
static uint32_t last_retry,last_sample;
static bool readreg(uint8_t address,uint8_t reg,uint8_t *data,unsigned size) {
    return HAL_I2C_Mem_Read(&bus,(uint16_t)address<<1,reg,I2C_MEMADD_SIZE_8BIT,data,(uint16_t)size,2)==HAL_OK;
}
static bool writereg(uint8_t address,uint8_t reg,uint8_t value) {
    return HAL_I2C_Mem_Write(&bus,(uint16_t)address<<1,reg,I2C_MEMADD_SIZE_8BIT,&value,1,2)==HAL_OK;
}
static bool oled_command(const uint8_t *cmd,unsigned n) {
    uint8_t data[32]={0}; if(n>31) return false; memcpy(data+1,cmd,n);
    return HAL_I2C_Master_Transmit(&bus,0x78,data,(uint16_t)(n+1),2)==HAL_OK;
}
static void magnetometer_init(void) {
    mag_id=0; mag_address=0;
    /* SDK boards normally carry LSM9DS1; identify instead of guessing revision. */
    const uint8_t addresses[]={0x1c,0x1e,0x1d};
    for(unsigned i=0;i<sizeof addresses;++i) {
        uint8_t id=0;
        if(!readreg(addresses[i],0x0f,&id,1)) continue;
        if(id==0x3d) {
            if(!writereg(addresses[i],0x20,0x7c)||!writereg(addresses[i],0x21,0x00)||
               !writereg(addresses[i],0x22,0x00)||!writereg(addresses[i],0x23,0x0c)||!writereg(addresses[i],0x24,0x40)) continue;
            sensitivity=14615;
        } else if(id==0x49) {
            if(!writereg(addresses[i],0x20,0x08)||!writereg(addresses[i],0x24,0x74)||
               !writereg(addresses[i],0x25,0)||!writereg(addresses[i],0x26,0)) continue;
            sensitivity=8000;
        } else continue;
        mag_address=addresses[i]; mag_id=id; last_sample=HAL_GetTick(); return;
    }
}
static void oled_init(void) {
    oled_ok=false; chunk=0;
    uint8_t config,output;
    if(!readreg(0x70,3,&config,1)||!readreg(0x70,1,&output,1)) return;
    /* Preserve other expander pins; P0 alone controls OLED reset. */
    if(!writereg(0x70,1,output|1)||!writereg(0x70,3,config&0xfe)) return;
    HAL_Delay(1);
    if(!writereg(0x70,1,output&0xfe)) return;
    HAL_Delay(1);
    if(!writereg(0x70,1,output|1)) return;
    HAL_Delay(2);
    const uint8_t init[]={0xae,0x20,0x00,0xc8,0x40,0x81,0xff,0xa1,0xa6,0xa8,0x3f,0xa4,0xd3,0,0xd5,0xf0,0xd9,0x22,0xda,0x12,0xdb,0x20,0x8d,0x14,0xaf};
    oled_ok=oled_command(init,sizeof init);
}
void devices_init(void) {
    __HAL_RCC_GPIOB_CLK_ENABLE(); __HAL_RCC_I2C1_CLK_ENABLE();
    GPIO_InitTypeDef io={0}; io.Pin=GPIO_PIN_8|GPIO_PIN_9; io.Mode=GPIO_MODE_AF_OD;
    io.Pull=GPIO_NOPULL; io.Speed=GPIO_SPEED_FREQ_VERY_HIGH; io.Alternate=GPIO_AF4_I2C1;
    HAL_GPIO_Init(GPIOB,&io);
    bus.Instance=I2C1; bus.Init.ClockSpeed=400000; bus.Init.DutyCycle=I2C_DUTYCYCLE_2;
    bus.Init.AddressingMode=I2C_ADDRESSINGMODE_7BIT;
    if(HAL_I2C_Init(&bus)!=HAL_OK) fatal();
    magnetometer_init(); oled_init(); last_retry=HAL_GetTick();
}
void devices_retry(uint32_t now) {
    if((uint32_t)(now-last_retry)<1000) return;
    last_retry=now;
    if(!mag_address) magnetometer_init();
    if(!oled_ok) oled_init();
}
void devices_sample(Compass *c) {
    c->tick=HAL_GetTick(); c->sensor_id=mag_id; c->display_ok=oled_ok;
    if(!mag_address) { c->sensor_ok=false; c->heading=-1; return; }
    uint8_t status;
    if(!readreg(mag_address,mag_id==0x3d?0x27:0x07,&status,1)) {
        c->sensor_ok=false; c->heading=-1; mag_address=0; return;
    }
    if(!(status&8)) {
        if((uint32_t)(HAL_GetTick()-last_sample)>500) { c->sensor_ok=false; c->heading=-1; mag_address=0; }
        return;
    }
    uint8_t data[6];
    if(!readreg(mag_address,(mag_id==0x3d?0x28:0x08)|0x80,data,6)) {
        c->sensor_ok=false; c->heading=-1; mag_address=0; return;
    }
    int16_t raw[3];
    for(unsigned i=0;i<3;++i) raw[i]=(int16_t)((uint16_t)data[2*i]|((uint16_t)data[2*i+1]<<8));
    compass_measure(c,raw,sensitivity); c->sensor_ok=true; last_sample=HAL_GetTick();
}
static void pixel(int x,int y) {
    if(x>=0&&x<128&&y>=0&&y<64) framebuffer[x+(y/8)*128]|=(uint8_t)(1u<<(y%8));
}
static void line(int x0,int y0,int x1,int y1) {
    int dx=x1>x0?x1-x0:x0-x1, sx=x0<x1?1:-1;
    int dy=y1>y0?y0-y1:y1-y0, sy=y0<y1?1:-1, e=dx+dy;
    for(;;) { pixel(x0,y0); if(x0==x1&&y0==y1) break; int twice=2*e;
        if(twice>=dy) { e+=dy; x0+=sx; } if(twice<=dx) { e+=dx; y0+=sy; }
    }
}
/* Compact original 3x5 font, rows encoded in low 3 bits. */
static const char alphabet[]="0123456789-+:.XYZNHFSEOR%? ";
static const uint8_t font[][5]={
 {7,5,5,5,7},{2,6,2,2,7},{7,1,7,4,7},{7,1,7,1,7},{5,5,7,1,1},
 {7,4,7,1,7},{7,4,7,5,7},{7,1,2,2,2},{7,5,7,5,7},{7,5,7,1,7},
 {0,0,7,0,0},{0,2,7,2,0},{0,2,0,2,0},{0,0,0,0,2},
 {5,5,2,5,5},{5,5,2,2,2},{7,1,2,4,7},{5,7,7,7,5},{5,5,7,5,5},
 {7,4,6,4,4},{7,4,7,1,7},{7,4,6,4,7},{7,5,5,5,7},{6,5,6,5,5},
 {5,1,2,4,5},{7,1,2,0,2},{0,0,0,0,0}};
static void text(int x,int y,const char *s) {
    for(;*s;s++,x+=4) { const char *p=strchr(alphabet,*s); unsigned index=p?(unsigned)(p-alphabet):25;
        for(int row=0;row<5;++row) for(int col=0;col<3;++col) if(font[index][row]&(1<<(2-col))) pixel(x+col,y+row);
    }
}
void display_draw(const Compass *c) {
    memset(framebuffer,0,sizeof framebuffer); char s[32];
    for(unsigned i=0;i<3;++i) {
        int32_t v=c->field[i];
        snprintf(s,sizeof s,"%c:%ld.%03ld",(int)("XYZ"[i]),(long)(v/1000),(long)(v<0?-(v%1000):v%1000));
        /* Preserve sign for negative values smaller than one microtesla. */
        if(v<0&&v>-1000) snprintf(s,sizeof s,"%c:-0.%03ld",(int)("XYZ"[i]),(long)-v);
        text(0,(int)i*10,s);
    }
    snprintf(s,sizeof s,"%u %u %u%%",c->scale[0],c->scale[1],c->scale[2]); text(0,34,s);
    if(c->sensor_ok&&c->heading>=0) {
        snprintf(s,sizeof s,"H:%d.%02d",c->heading/100,c->heading%100); text(0,44,s);
        float a=(float)c->heading*0.0001745329252f;
        int dx=(int)lroundf(sinf(a)*21),dy=-(int)lroundf(cosf(a)*21);
        line(101,33,101+dx,33+dy);
        line(101+dx,33+dy,101+dx-dx/3+dy/4,33+dy-dy/3-dx/4);
        line(101+dx,33+dy,101+dx-dx/3-dy/4,33+dy-dy/3+dx/4);
    } else text(0,44,c->sensor_ok?"H:--":"SENSOR ERROR");
    snprintf(s,sizeof s,"F:%02lu",(unsigned long)(c->frames%100)); text(0,56,s); text(99,3,"N");
    const uint8_t address[]={0x21,0,127,0x22,0,7};
    if(oled_ok&&!oled_command(address,sizeof address)) oled_ok=false;
    chunk=0;
}
bool display_chunk(void) {
    if(!oled_ok||chunk>=32) return false;
    uint8_t data[33]; data[0]=0x40; memcpy(data+1,framebuffer+(unsigned)chunk*32,32);
    if(HAL_I2C_Master_Transmit(&bus,0x78,data,sizeof data,2)!=HAL_OK) { oled_ok=false; return false; }
    return ++chunk==32;
}
bool display_ready(void) { return oled_ok; }
