#include "board.h"
#include "devices.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
static uint8_t registers[128][256];
static uint32_t ticks;
static unsigned oled_chunks,oled_commands;
static bool bus_fault;
uint32_t HAL_GetTick(void) { return ticks; }
void HAL_Delay(uint32_t n) { ticks+=n; }
void HAL_GPIO_Init(void *p,GPIO_InitTypeDef *io) { assert(p==GPIOB&&io->Pin==(GPIO_PIN_8|GPIO_PIN_9)&&io->Mode==GPIO_MODE_AF_OD); }
HAL_StatusTypeDef HAL_I2C_Init(I2C_HandleTypeDef *h) { assert(h->Init.ClockSpeed==400000);return HAL_OK; }
HAL_StatusTypeDef HAL_I2C_Mem_Read(I2C_HandleTypeDef *h,uint16_t a,uint16_t r,unsigned rs,uint8_t *d,uint16_t n,unsigned timeout) {
    (void)h; assert(rs==1&&timeout==2&&(a&1)==0); if(bus_fault) return HAL_ERROR;
    a>>=1;r&=0x7f;
    for(unsigned i=0;i<n;++i) d[i]=registers[a][r+i];return HAL_OK;
}
HAL_StatusTypeDef HAL_I2C_Mem_Write(I2C_HandleTypeDef *h,uint16_t a,uint16_t r,unsigned rs,uint8_t *d,uint16_t n,unsigned timeout) {
    (void)h; assert(rs==1&&timeout==2&&n==1&&(a&1)==0);if(bus_fault) return HAL_ERROR;
    registers[a>>1][r]=*d;return HAL_OK;
}
HAL_StatusTypeDef HAL_I2C_Master_Transmit(I2C_HandleTypeDef *h,uint16_t a,uint8_t *d,uint16_t n,unsigned timeout) {
    (void)h;assert(a==0x78&&timeout==2);if(bus_fault) return HAL_ERROR;
    if(d[0]==0x40) { assert(n==33); ++oled_chunks; } else { assert(d[0]==0&&n<=32);++oled_commands; }
    return HAL_OK;
}
static void setup(uint8_t id,uint8_t addr) {
    memset(registers,0,sizeof registers);registers[addr][0x0f]=id;
    registers[0x70][3]=0xf7; registers[0x70][1]=0xa6;
    ticks=0;bus_fault=false;oled_chunks=oled_commands=0; devices_init();
    assert(registers[0x70][3]==0xf6);assert(registers[0x70][1]==0xa7);
    assert(display_ready());assert(oled_commands==1);
}
int main(void) {
    Compass c;
    setup(0x3d,0x1e);compass_init(&c);
    assert(registers[0x1e][0x20]==0x7c&&registers[0x1e][0x24]==0x40);
    registers[0x1e][0x27]=8;registers[0x1e][0x28]=1;
    devices_sample(&c);assert(c.sensor_ok&&c.sensor_id==0x3d&&c.field[0]==14&&c.heading==0);
    display_draw(&c);for(unsigned i=0;i<32;++i) assert(display_chunk()==(i==31));assert(oled_chunks==32);assert(!display_chunk());
    registers[0x1e][0x27]=0;ticks=600;devices_sample(&c);assert(!c.sensor_ok&&c.heading==-1);
    setup(0x49,0x1d);compass_init(&c);registers[0x1d][0x07]=8;registers[0x1d][0x08]=2;
    devices_sample(&c);assert(c.sensor_ok&&c.sensor_id==0x49&&c.field[0]==16);
    bus_fault=true;devices_sample(&c);assert(!c.sensor_ok&&c.heading==-1);
    display_draw(&c);assert(!display_ready());
    bus_fault=false;ticks=2000;devices_retry(ticks);assert(display_ready());
    puts("PASS I2C/OLED mocks: 7-bit addresses, DS1/DS0 identification, BDU, reset preserves pins, 1024-byte frame, stale data, faults/retry");
}
