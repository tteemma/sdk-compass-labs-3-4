#include "board.h"
#include "serial.h"
#include "devices.h"
#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"
static QueueHandle_t scale_queue,status_queue;
void vApplicationMallocFailedHook(void) { fatal(); }
void vApplicationStackOverflowHook(TaskHandle_t task,char *name) { (void)task; (void)name; fatal(); }
/* This task alone owns I2C, OLED framebuffer, sensor and authoritative scales. */
static void display_task(void *arg) {
    (void)arg; Compass c; compass_init(&c); devices_init();
    TickType_t wake=xTaskGetTickCount(); uint32_t previous=HAL_GetTick();
    for(;;) {
        unsigned scale[3];
        if(xQueueReceive(scale_queue,scale,0)==pdTRUE) compass_scale(&c,scale[0],scale[1],scale[2]);
        devices_retry(HAL_GetTick()); devices_sample(&c); display_draw(&c);
        for(unsigned part=0;part<32 && display_ready();++part) {
            if(display_chunk()) { ++c.frames; c.frame_ms=HAL_GetTick()-previous; previous=HAL_GetTick(); }
        }
        c.display_ok=display_ready();
        xQueueOverwrite(status_queue,&c);
        xTaskDelayUntil(&wake,pdMS_TO_TICKS(40));
    }
}
/* Only this task calls foreground UART functions; ISR is the queue counterpart. */
static void communications_task(void *arg) {
    (void)arg; Compass latest; compass_init(&latest);
    char input[80]; unsigned used=0; bool overflow=false;
    uint32_t report=HAL_GetTick();
    serial_write("READY COMPASS 1 LR4 FREERTOS\r\n");
    for(;;) {
        serial_service();
        (void)xQueuePeek(status_queue,&latest,0);
        uint8_t b;
        for(unsigned budget=0;budget<32 && serial_tx_space()>180 && serial_read(&b);++budget) {
            if(b=='\r') continue;
            if(b=='\n') {
                input[used]=0; unsigned scale[3]; int cmd=overflow?0:compass_command(input,scale);
                if(cmd==1) serial_write("PONG COMPASS 1\r\n");
                else if(cmd==2) { char reply[180]; compass_status(&latest,reply,sizeof reply); serial_write(reply); }
                else if(cmd==3) { xQueueOverwrite(scale_queue,scale); serial_write("OK SCALE QUEUED\r\n"); }
                else serial_write("ERR COMMAND\r\n");
                used=0; overflow=false;
            } else if(used<sizeof input-1) input[used++]=(char)b; else overflow=true;
        }
        if((uint32_t)(HAL_GetTick()-report)>=100 && serial_tx_space()>180) {
            char reply[180]; compass_status(&latest,reply,sizeof reply); serial_write(reply); report=HAL_GetTick();
        }
        vTaskDelay(pdMS_TO_TICKS(1));
    }
}
int main(void) {
    board_init(); serial_init(115200);
    scale_queue=xQueueCreate(1,sizeof(unsigned[3])); status_queue=xQueueCreate(1,sizeof(Compass));
    if(!scale_queue||!status_queue) fatal();
    if(xTaskCreate(display_task,"display",1024,0,2,0)!=pdPASS) fatal();
    if(xTaskCreate(communications_task,"uart",768,0,3,0)!=pdPASS) fatal();
    vTaskStartScheduler(); fatal();
}
