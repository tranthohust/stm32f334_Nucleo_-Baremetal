#include "board_peripherals.h"
#include "io_demo.h"
#if FIRMWARE_USE_FREERTOS
#include "rtos_demo.h"
#endif

/* Ten dung theo startup vector. ISR chi chuyen handle cho driver,
 * giong GPIO_IRQHandling: khong parse command/printf/callback ung dung. */
void USART2_IRQHandler(void)
{
    UART_IRQHandling(&huart2);
}

void DMA1_Channel6_IRQHandler(void)
{
    UART_DMARxIRQHandling(&huart2);
}

void DMA1_Channel7_IRQHandler(void)
{
    UART_DMATxIRQHandling(&huart2);
}

void SPI1_IRQHandler(void)
{
    SPI_IRQHandling(&hspi1);
}

void CAN_TX_IRQHandler(void)
{
    CAN_TXIRQHandling(&hcan1);
}

void CAN_RX0_IRQHandler(void)
{
    CAN_RX0IRQHandling(&hcan1);
}

void CAN_RX1_IRQHandler(void)
{
    CAN_RX1IRQHandling(&hcan1);
}

void CAN_SCE_IRQHandler(void)
{
    CAN_SCEIRQHandling(&hcan1);
}

void I2C1_EV_IRQHandler(void)
{
    I2C_EVIRQHandling(&hi2c1);
#if FIRMWARE_USE_FREERTOS
    RTOS_I2CNotifyFromISR(); /* Driver da khoi phuc PRIMASK; IRQ priority6. */
#endif
}

void I2C1_ER_IRQHandler(void)
{
    I2C_ERIRQHandling(&hi2c1);
#if FIRMWARE_USE_FREERTOS
    RTOS_I2CNotifyFromISR();
#endif
}

void TIM3_IRQHandler(void) { TIM_IRQHandling(&htim3); }
void ADC1_2_IRQHandler(void) { ADC_IRQHandling(&hadc1); }
void EXTI0_IRQHandler(void) { IO_Demo_GPIOIRQ(); }
