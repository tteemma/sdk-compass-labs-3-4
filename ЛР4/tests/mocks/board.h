#ifndef TEST_BOARD_H
#define TEST_BOARD_H
#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h>
typedef enum { HAL_OK, HAL_ERROR } HAL_StatusTypeDef;
typedef struct { unsigned ClockSpeed,DutyCycle,AddressingMode; } TestInit;
typedef struct { void *Instance; TestInit Init; } I2C_HandleTypeDef;
typedef struct { unsigned Pin,Mode,Pull,Speed,Alternate; } GPIO_InitTypeDef;
#define I2C1 ((void *)1)
#define GPIOB ((void *)2)
#define GPIO_PIN_8 256
#define GPIO_PIN_9 512
#define GPIO_MODE_AF_OD 1
#define GPIO_NOPULL 0
#define GPIO_SPEED_FREQ_VERY_HIGH 3
#define GPIO_AF4_I2C1 4
#define I2C_MEMADD_SIZE_8BIT 1
#define I2C_DUTYCYCLE_2 2
#define I2C_ADDRESSINGMODE_7BIT 7
#define __HAL_RCC_GPIOB_CLK_ENABLE() ((void)0)
#define __HAL_RCC_I2C1_CLK_ENABLE() ((void)0)
uint32_t HAL_GetTick(void);
void HAL_Delay(uint32_t n);
void HAL_GPIO_Init(void *p,GPIO_InitTypeDef *io);
HAL_StatusTypeDef HAL_I2C_Init(I2C_HandleTypeDef *h);
HAL_StatusTypeDef HAL_I2C_Mem_Read(I2C_HandleTypeDef *h,uint16_t a,uint16_t r,unsigned rs,uint8_t *d,uint16_t n,unsigned timeout);
HAL_StatusTypeDef HAL_I2C_Mem_Write(I2C_HandleTypeDef *h,uint16_t a,uint16_t r,unsigned rs,uint8_t *d,uint16_t n,unsigned timeout);
HAL_StatusTypeDef HAL_I2C_Master_Transmit(I2C_HandleTypeDef *h,uint16_t a,uint8_t *d,uint16_t n,unsigned timeout);
static inline void fatal(void) { abort(); }
#endif
