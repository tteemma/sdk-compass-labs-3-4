# MCU=STM32F407xx also supported. UART_BACKEND=REGISTER builds LR2 part 2.
IDE_APP ?= $(firstword $(wildcard ../../../STM32CubeIDE.app /Applications/STM32CubeIDE.app))
TOOL_BIN := $(firstword $(wildcard $(IDE_APP)/Contents/Eclipse/plugins/com.st.stm32cube.ide.mcu.externaltools.gnu-tools-for-stm32*/tools/bin))
LOCAL_TOOL := $(abspath ../../tools/toolchain/bin/arm-none-eabi-)
CROSS ?= $(if $(TOOL_BIN),$(TOOL_BIN)/arm-none-eabi-,$(if $(wildcard $(LOCAL_TOOL)gcc),$(LOCAL_TOOL),arm-none-eabi-))
CC := $(CROSS)gcc
MCU ?= STM32F427xx
UART_BACKEND ?= HAL
TARGET := Debug/Compass_RTOS_LR4_V2_$(UART_BACKEND)_$(MCU)
INCLUDES := -ICore/Inc -IDrivers/HAL/Inc -IDrivers/CMSIS/Device/Include -IDrivers/CMSIS/Core/Include
FLAGS := -mcpu=cortex-m4 -mthumb -mfpu=fpv4-sp-d16 -mfloat-abi=softfp
CFLAGS := $(FLAGS) -std=c11 -Os -g3 -Wall -Wextra -Werror -ffunction-sections -fdata-sections -D$(MCU) -DUSE_HAL_DRIVER $(INCLUDES)
LDFLAGS := $(FLAGS) -nostartfiles --specs=nosys.specs -TSTM32F4_FLASH.ld -Wl,--gc-sections,-Map=$(TARGET).map
HAL_NAMES := hal hal_dma hal_rcc hal_rcc_ex hal_gpio hal_cortex hal_flash hal_flash_ex hal_pwr hal_pwr_ex hal_i2c
ifeq ($(UART_BACKEND),HAL)
HAL_NAMES += hal_uart
else
CFLAGS += -DUART_REGISTER_BACKEND
endif
SOURCES := $(wildcard Core/Src/*.c) $(addprefix Drivers/HAL/Src/stm32f4xx_,$(addsuffix .c,$(HAL_NAMES)))
CFLAGS += -IMiddlewares/FreeRTOS/include -IMiddlewares/FreeRTOS/portable/GCC/ARM_CM4F -DUSE_FREERTOS
SOURCES += Middlewares/FreeRTOS/tasks.c Middlewares/FreeRTOS/queue.c Middlewares/FreeRTOS/list.c Middlewares/FreeRTOS/portable/GCC/ARM_CM4F/port.c Middlewares/FreeRTOS/portable/MemMang/heap_4.c

OBJECTS := $(addprefix $(TARGET)_obj/,$(SOURCES:.c=.o)) $(TARGET)_obj/startup.o
.PHONY: all clean
all: $(TARGET).elf $(TARGET).hex $(TARGET).bin
	$(CROSS)size $(TARGET).elf
$(TARGET).elf: $(OBJECTS) STM32F4_FLASH.ld
	$(CC) $(LDFLAGS) $(OBJECTS) -Wl,--start-group -lc -lm -lgcc -lnosys -Wl,--end-group -o $@
# Vendor HAL has intentionally unused bank parameters on single-bank F407.
$(filter %/Drivers/HAL/Src/stm32f4xx_hal_flash_ex.o,$(OBJECTS)): CFLAGS += -Wno-unused-parameter
$(TARGET)_obj/%.o: %.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -MMD -MP -c $< -o $@
$(TARGET)_obj/startup.o: Startup/startup_stm32f4.s
	@mkdir -p $(dir $@)
	$(CC) $(FLAGS) -g3 -c $< -o $@
$(TARGET).hex: $(TARGET).elf
	$(CROSS)objcopy -O ihex $< $@
$(TARGET).bin: $(TARGET).elf
	$(CROSS)objcopy -O binary $< $@
clean:
	rm -rf Debug
-include $(OBJECTS:.o=.d)
