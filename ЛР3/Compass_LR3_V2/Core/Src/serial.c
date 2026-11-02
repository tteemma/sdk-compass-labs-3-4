#include "serial.h"
#include "board.h"
#include "fifo.h"
#include <string.h>
static Fifo rx, tx;
static bool interrupt_mode=true;
static int pending_mode=-1;
volatile uint32_t serial_rx_dropped, serial_errors, serial_tx_dropped;
#ifndef UART_REGISTER_BACKEND
static UART_HandleTypeDef uart;
static uint8_t rx_byte, tx_byte;
static volatile bool tx_busy;
void HAL_UART_MspInit(UART_HandleTypeDef *h) { (void)h; }
void HAL_UART_RxCpltCallback(UART_HandleTypeDef *h) {
    if (h != &uart) return;
    if (!fifo_put(&rx,rx_byte)) ++serial_rx_dropped;
    (void)HAL_UART_Receive_IT(&uart,&rx_byte,1);
}
void HAL_UART_TxCpltCallback(UART_HandleTypeDef *h) {
    if(h != &uart) return;
    if(fifo_get(&tx,&tx_byte)) {
        if(HAL_UART_Transmit_IT(&uart,&tx_byte,1)!=HAL_OK) { tx_busy=false; ++serial_errors; }
    } else tx_busy=false;
}
void HAL_UART_ErrorCallback(UART_HandleTypeDef *h) {
    if (h != &uart) return;
    ++serial_errors;
    if (interrupt_mode && h->RxState == HAL_UART_STATE_READY)
        (void)HAL_UART_Receive_IT(h,&rx_byte,1);
}
#endif
static void set_mode(bool enable)
{
    HAL_NVIC_DisableIRQ(USART6_IRQn);
#ifndef UART_REGISTER_BACKEND
    (void)HAL_UART_AbortReceive(&uart);
#else
    USART6->CR1 &= ~(USART_CR1_RXNEIE | USART_CR1_TXEIE | USART_CR1_PEIE);
    USART6->CR3 &= ~USART_CR3_EIE;
#endif
    interrupt_mode=enable;
    HAL_NVIC_ClearPendingIRQ(USART6_IRQn);
    if (enable) {
#ifndef UART_REGISTER_BACKEND
        (void)HAL_UART_Receive_IT(&uart,&rx_byte,1);
#else
        USART6->CR1 |= USART_CR1_RXNEIE;
        USART6->CR3 |= USART_CR3_EIE;
#endif
        HAL_NVIC_EnableIRQ(USART6_IRQn);
    }
}
void serial_init(uint32_t baud)
{
    __HAL_RCC_USART6_CLK_ENABLE(); __HAL_RCC_GPIOC_CLK_ENABLE();
    GPIO_InitTypeDef io={0}; io.Pin=GPIO_PIN_6|GPIO_PIN_7;
    io.Mode=GPIO_MODE_AF_PP; io.Pull=GPIO_PULLUP;
    io.Speed=GPIO_SPEED_FREQ_VERY_HIGH; io.Alternate=GPIO_AF8_USART6;
    HAL_GPIO_Init(GPIOC,&io);
#ifndef UART_REGISTER_BACKEND
    uart.Instance=USART6; uart.Init.BaudRate=baud;
    uart.Init.WordLength=UART_WORDLENGTH_8B; uart.Init.StopBits=UART_STOPBITS_1;
    uart.Init.Parity=UART_PARITY_NONE; uart.Init.Mode=UART_MODE_TX_RX;
    uart.Init.HwFlowCtl=UART_HWCONTROL_NONE; uart.Init.OverSampling=UART_OVERSAMPLING_16;
    if (HAL_UART_Init(&uart) != HAL_OK) fatal();
#else
    USART6->CR1=0; USART6->CR2=0; USART6->CR3=0;
    USART6->BRR=(HAL_RCC_GetPCLK2Freq()+baud/2u)/baud;
    USART6->CR1=USART_CR1_UE|USART_CR1_RE|USART_CR1_TE;
#endif
    HAL_NVIC_SetPriority(USART6_IRQn,6,0); set_mode(true);
}
void USART6_IRQHandler(void)
{
#ifndef UART_REGISTER_BACKEND
    HAL_UART_IRQHandler(&uart);
#else
    uint32_t status=USART6->SR;
    if (status & (USART_SR_RXNE|USART_SR_ORE|USART_SR_FE|USART_SR_NE|USART_SR_PE)) {
        uint8_t c=(uint8_t)USART6->DR;
        if (status & (USART_SR_ORE|USART_SR_FE|USART_SR_NE|USART_SR_PE)) ++serial_errors;
        if ((status & USART_SR_RXNE) && !fifo_put(&rx,c)) ++serial_rx_dropped;
    }
    if ((status & USART_SR_TXE) && (USART6->CR1 & USART_CR1_TXEIE)) {
        uint8_t c;
        if (fifo_get(&tx,&c)) USART6->DR=c;
        else USART6->CR1 &= ~USART_CR1_TXEIE;
    }
#endif
}
void serial_service(void)
{
    uint8_t c;
    if (!interrupt_mode) {
#ifndef UART_REGISTER_BACKEND
        if (__HAL_UART_GET_FLAG(&uart,UART_FLAG_RXNE) && HAL_UART_Receive(&uart,&c,1,0)==HAL_OK)
            if (!fifo_put(&rx,c)) ++serial_rx_dropped;
        if (__HAL_UART_GET_FLAG(&uart,UART_FLAG_ORE)) { __HAL_UART_CLEAR_OREFLAG(&uart); ++serial_errors; }
        if (__HAL_UART_GET_FLAG(&uart,UART_FLAG_TXE) && fifo_get(&tx,&c))
            if (HAL_UART_Transmit(&uart,&c,1,2)!=HAL_OK) ++serial_errors;
#else
        uint32_t status=USART6->SR;
        if (status & (USART_SR_RXNE|USART_SR_ORE|USART_SR_FE|USART_SR_NE|USART_SR_PE)) {
            c=(uint8_t)USART6->DR;
            if (status & (USART_SR_ORE|USART_SR_FE|USART_SR_NE|USART_SR_PE)) ++serial_errors;
            if ((status & USART_SR_RXNE) && !fifo_put(&rx,c)) ++serial_rx_dropped;
        }
        if ((status & USART_SR_TXE) && fifo_get(&tx,&c)) USART6->DR=c;
#endif
    } else {
#ifndef UART_REGISTER_BACKEND
        uint32_t mask=__get_PRIMASK(); __disable_irq();
        if (!tx_busy && fifo_get(&tx,&tx_byte)) {
            tx_busy=true;
            if (HAL_UART_Transmit_IT(&uart,&tx_byte,1)!=HAL_OK) { tx_busy=false; ++serial_errors; }
        }
        __set_PRIMASK(mask);
#else
        if (!fifo_empty(&tx)) USART6->CR1 |= USART_CR1_TXEIE;
#endif
    }
    bool idle=fifo_empty(&tx) && (USART6->SR & USART_SR_TC);
#ifndef UART_REGISTER_BACKEND
    idle=idle && !tx_busy;
#endif
    if (pending_mode>=0 && idle) {
        set_mode(pending_mode!=0); pending_mode=-1;
        (void)serial_write(interrupt_mode ? "\r\nOK interrupts on\r\n" : "\r\nOK interrupts off\r\n");
    }
}
bool serial_read(uint8_t *c) { return fifo_get(&rx,c); }
bool serial_write(const char *s) {
    size_t len=strlen(s);
    if (len>fifo_space(&tx)) { ++serial_tx_dropped; return false; }
    while (*s) (void)fifo_put(&tx,(uint8_t)*s++);
    return true;
}
unsigned serial_tx_space(void) { return fifo_space(&tx); }
void serial_request_mode(bool enable) { pending_mode=enable ? 1 : 0; }
bool serial_interrupts(void) { return interrupt_mode; }
