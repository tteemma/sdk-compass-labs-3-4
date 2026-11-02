#ifndef COMPASS_H
#define COMPASS_H
#include <stdint.h>
#include <stdbool.h>
typedef struct {
    int32_t field[3]; /* nanotesla after scale */
    uint16_t scale[3];
    int heading; /* centidegrees; -1 means no horizontal field */
    uint32_t tick,frames,frame_ms;
    uint8_t sensor_id;
    bool sensor_ok,display_ok;
} Compass;
void compass_init(Compass *c);
bool compass_scale(Compass *c,unsigned x,unsigned y,unsigned z);
void compass_measure(Compass *c,const int16_t raw[3],int sensitivity_nt_x1000);
int compass_heading(int32_t x,int32_t y);
/* ASCII newline protocol; rejects extra text and out-of-range values. */
int compass_command(const char *line,unsigned scales[3]);
void compass_status(const Compass *c,char *out,unsigned size);
#endif
