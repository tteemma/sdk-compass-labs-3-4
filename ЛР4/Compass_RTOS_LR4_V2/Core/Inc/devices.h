#ifndef DEVICES_H
#define DEVICES_H
#include "compass.h"
void devices_init(void);
void devices_sample(Compass *c);
void display_draw(const Compass *c);
bool display_chunk(void); /* 32 chunks; true only on successful complete frame */
bool display_ready(void);
void devices_retry(uint32_t now);
#endif
