#include "board.h"
#include "serial.h"
#include "devices.h"
#include <string.h>
static Compass compass;
static char input[80];
static unsigned used;
static bool overflow;
static void receive(void) {
    uint8_t b;
    while(serial_tx_space()>180&&serial_read(&b)) {
        if(b=='\r') continue;
        if(b=='\n') {
            input[used]=0; unsigned scale[3]; int cmd=overflow?0:compass_command(input,scale);
            if(cmd==1) serial_write("PONG COMPASS 1\r\n");
            else if(cmd==2) { char reply[180]; compass_status(&compass,reply,sizeof reply); serial_write(reply); }
            else if(cmd==3) { compass_scale(&compass,scale[0],scale[1],scale[2]); serial_write("OK SCALE\r\n"); }
            else serial_write("ERR COMMAND\r\n");
            used=0; overflow=false;
        } else if(used<sizeof input-1) input[used++]=(char)b; else overflow=true;
    }
}
int main(void) {
    board_init(); serial_init(115200); compass_init(&compass); devices_init();
    uint32_t frame_start=HAL_GetTick(),last_frame=frame_start,last_report=frame_start;
    bool drawing=false; unsigned parts=0;
    serial_write("READY COMPASS 1 LR3\r\n");
    for(;;) {
        serial_service(); receive(); uint32_t now=HAL_GetTick();
        if(!drawing && (uint32_t)(now-frame_start)>=40) {
            frame_start=now; devices_retry(now); devices_sample(&compass);
            display_draw(&compass); drawing=display_ready(); parts=0;
        }
        if(drawing) {
            if(display_chunk()) { ++compass.frames; compass.frame_ms=HAL_GetTick()-last_frame; last_frame=HAL_GetTick(); }
            ++parts; if(parts==32||!display_ready()) drawing=false;
        }
        compass.display_ok=display_ready();
        if((uint32_t)(now-last_report)>=100 && serial_tx_space()>180) {
            char reply[180]; compass_status(&compass,reply,sizeof reply); serial_write(reply); last_report=now;
        }
    }
}
