#include "board.h"
void fatal(void) { __disable_irq(); for (;;) {} }
HAL_StatusTypeDef HAL_InitTick(uint32_t priority)
{
    __HAL_RCC_TIM6_CLK_ENABLE();
    uint32_t clock = HAL_RCC_GetPCLK1Freq();
    if ((RCC->CFGR & RCC_CFGR_PPRE1) != 0) clock *= 2;
    TIM6->CR1 = 0;
    TIM6->PSC = clock / 1000000u - 1u;
    TIM6->ARR = 999u;
    TIM6->EGR = TIM_EGR_UG;
    TIM6->SR = 0;
    TIM6->DIER = TIM_DIER_UIE;
    HAL_NVIC_SetPriority(TIM6_DAC_IRQn, priority, 0);
    HAL_NVIC_EnableIRQ(TIM6_DAC_IRQn);
    TIM6->CR1 = TIM_CR1_CEN;
    return HAL_OK;
}
void TIM6_DAC_IRQHandler(void) { TIM6->SR = 0; HAL_IncTick(); }
void board_init(void)
{
    SCB->VTOR = FLASH_BASE;
    HAL_Init();
    __HAL_RCC_PWR_CLK_ENABLE();
    __HAL_PWR_VOLTAGESCALING_CONFIG(PWR_REGULATOR_VOLTAGE_SCALE1);
    RCC_OscInitTypeDef osc = {0};
    osc.OscillatorType = RCC_OSCILLATORTYPE_HSI;
    osc.HSIState = RCC_HSI_ON;
    osc.HSICalibrationValue = RCC_HSICALIBRATION_DEFAULT;
    osc.PLL.PLLState = RCC_PLL_ON;
    osc.PLL.PLLSource = RCC_PLLSOURCE_HSI;
    osc.PLL.PLLM = 16; osc.PLL.PLLN = 336;
    osc.PLL.PLLP = RCC_PLLP_DIV4; osc.PLL.PLLQ = 7;
    if (HAL_RCC_OscConfig(&osc) != HAL_OK) fatal();
    RCC_ClkInitTypeDef clk = {0};
    clk.ClockType = RCC_CLOCKTYPE_SYSCLK | RCC_CLOCKTYPE_HCLK | RCC_CLOCKTYPE_PCLK1 | RCC_CLOCKTYPE_PCLK2;
    clk.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
    clk.AHBCLKDivider = RCC_SYSCLK_DIV1;
    clk.APB1CLKDivider = RCC_HCLK_DIV2;
    clk.APB2CLKDivider = RCC_HCLK_DIV1;
    if (HAL_RCC_ClockConfig(&clk, FLASH_LATENCY_2) != HAL_OK) fatal();
    __HAL_RCC_GPIOC_CLK_ENABLE(); __HAL_RCC_GPIOD_CLK_ENABLE();
    GPIO_InitTypeDef io = {0};
    io.Pin = GPIO_PIN_13 | GPIO_PIN_14 | GPIO_PIN_15;
    io.Mode = GPIO_MODE_OUTPUT_PP; io.Speed = GPIO_SPEED_FREQ_LOW;
    HAL_GPIO_Init(GPIOD, &io); board_leds(0);
    io.Pin = GPIO_PIN_15; io.Mode = GPIO_MODE_INPUT; io.Pull = GPIO_PULLUP;
    HAL_GPIO_Init(GPIOC, &io);
}
void board_leds(uint8_t colors)
{
    uint32_t pins = ((uint32_t)colors & 7u) << 13u;
    GPIOD->BSRR = ((0xE000u & ~pins) << 16u) | pins;
}
bool board_button(void) { return HAL_GPIO_ReadPin(GPIOC, GPIO_PIN_15) == GPIO_PIN_RESET; }
#ifndef USE_FREERTOS
void SysTick_Handler(void) {}
#endif
