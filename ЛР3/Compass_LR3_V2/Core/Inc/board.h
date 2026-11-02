#ifndef BOARD_H
#define BOARD_H
#include "stm32f4xx_hal.h"
#include <stdbool.h>
void board_init(void);
void board_leds(uint8_t colors);
bool board_button(void);
void fatal(void);
#endif
