#ifndef DEMO_CONFIG_H
#define DEMO_CONFIG_H

/* Bai hoc giu nguyen clock reset HSI: khong co PLL, prescaler deu /1.
 * Khi doi RCC phai cap nhat ca CPU, USART PCLK va CAN PCLK tai day. */
#define DEMO_CORE_HZ 8000000U
#define DEMO_APB1_HZ 8000000U
#define DEMO_TIMER_HZ 8000000U /* APB1 /1: timer=PCLK1. Neu APB chia, timer=2*PCLK1. */
#define DEMO_ADC_VREF_MV 3300U /* Gia tri gia dinh de in mV; do VDDA thuc te de chinh. */
#define DEMO_UART_BAUD 115200U
#define DEMO_CAN_BITRATE 125000U
#define DEMO_I2C_KERNEL_HZ 8000000U
/* Vi du doc 1 byte tai register 0 cua thiet bi 7-bit address 0x50.
 * Doi theo datasheet thiet bi cua ban. Khong gia dinh board da gan sensor. */
#define DEMO_I2C_ADDRESS 0x50U
#define DEMO_I2C_REGISTER 0x00U

/* 1: DMA circular RX + HT/TC/IDLE. 0: RXNE interrupt tung byte.
 * Ca hai deu dung TX DMA va chung line parser. Build lai sau khi doi. */
#define DEMO_UART_USE_DMA 1

/* Silent loopback: khong can transceiver/node CAN thu hai.
 * Doi thanh 0 de dung bus that; doc huong dan phan cung trong docs/. */
#define DEMO_CAN_LOOPBACK 1

#endif
