#ifndef STM32F334_UART_H
#define STM32F334_UART_H

#include "STM32F334.h"
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/* Bai hoc dung USART2: PA2=TX, PA3=RX, AF7; 8 data, no parity, 1 stop.
 * Ung dung cau hinh GPIO; UART driver chi quan ly USART/DMA/NVIC.
 * Cac API du lieu chi goi tu main; IRQHandling duoc goi tu ISR cua ung dung. */
typedef enum {
    UART_RX_IRQ = 0,
    UART_RX_DMA = 1
} UART_RxMode;

typedef struct {
    uint32_t UART_PCLKHz;
    uint32_t UART_BaudRate;
    UART_RxMode UART_RxMode;
    uint8_t UART_IRQPriority; /* 0..15; cung priority cho USART va DMA RX/TX. */
} UART_Config_t;

typedef struct {
    uint32_t rx_bytes;          /* Byte da lay tu RDR/DMA, ke ca byte bi drop. */
    uint32_t tx_bytes;          /* Byte DMA da ghi vao TDR, chua chac ra het day. */
    uint32_t rx_dropped;
    uint32_t rx_errors;
    uint32_t dma_errors;
    uint32_t rx_epoch;          /* Doi khi luong RX mat lien tuc; parser can resync. */
    uint32_t tx_dropped;        /* Byte bi tu choi do ring day + byte mat khi DMA loi. */
    uint32_t rx_dma_overruns;
} UART_Stats;

#define UART_RX_BUFFER_SIZE  512U
#define UART_TX_BUFFER_SIZE  256U
#define UART_DMA_BUFFER_SIZE 256U

typedef struct {
    USART_RegDef_t *pUSARTx;
    UART_Config_t UART_Config;

    /* Phan runtime thuoc RIENG handle nay. Khoi tao handle bang {0}, giu song
     * den khi DeInit; khong copy/sua cau hinh hay cac field runtime khi dang chay.
     * Buffer DMA phai o SRAM thuong, khong dat handle trong CCM hoac stack ngan. */
    uint8_t RxBuffer[UART_RX_BUFFER_SIZE];
    uint8_t TxBuffer[UART_TX_BUFFER_SIZE];
    volatile uint8_t RxDMABuffer[UART_DMA_BUFFER_SIZE] __attribute__((aligned(4)));
    volatile uint16_t RxHead, RxTail;
    volatile uint16_t TxHead, TxTail, TxActiveLength;
    volatile bool Initialized, TxBusy;
    UART_RxMode ActiveRxMode;
    volatile UART_Stats Stats;
    uint32_t DMAWrapBytes, DMAReadTotal;
} UART_Handle_t;

void UART_PeriClockControl(USART_RegDef_t *pUSARTx, uint8_t EnorDi);
bool UART_Init(UART_Handle_t *pUARTHandle);
/* DeInit chu dong abort/discard RX/TX dang cho, tat IRQ/DMA va clock USART.
 * Init tu choi neu USART2 da co owner, ke ca Init lai cung handle.
 * Muon doi cau hinh: DeInit -> sua Config -> Init. Init fail khong giu owner.
 * Hien chi ho tro pUSARTx == USART2; IRQ goi sai handle se khong truy cap MMIO. */
bool UART_DeInit(UART_Handle_t *pUARTHandle);

/* Ring RX 512/TX 256 byte, de trong mot o: dung luong 511/255 byte.
 * Write copy du lieu vao ring, nen caller duoc tai su dung input ngay.
 * Co the chi nhan mot phan: caller QUAN SAT return value.
 * Phan bi tu choi tang tx_dropped; caller duoc retry, counter dem moi lan tu choi. */
size_t UART_Write(UART_Handle_t *pUARTHandle, const uint8_t *data, size_t length);
size_t UART_Read(UART_Handle_t *pUARTHandle, uint8_t *data, size_t capacity);
void UART_Process(UART_Handle_t *pUARTHandle);
bool UART_TxIdle(UART_Handle_t *pUARTHandle);
/* Sai owner/null: GetStats khong sua out, Read/Write/Epoch tra 0, TxIdle false. */
void UART_GetStats(UART_Handle_t *pUARTHandle, UART_Stats *out);
uint32_t UART_RxEpoch(UART_Handle_t *pUARTHandle);
void UART_DiscardRx(UART_Handle_t *pUARTHandle);

void UART_IRQHandling(UART_Handle_t *pUARTHandle);
void UART_DMARxIRQHandling(UART_Handle_t *pUARTHandle);
void UART_DMATxIRQHandling(UART_Handle_t *pUARTHandle);

/* DMA circular 256 byte: phai phuc vu truoc khi DMA quay lai vung chua copy.
 * Tai 115200 8N1, 128 byte ~11.1ms: khong khoa IRQ lau hon moc nay.
 * Khong the khoi phuc/nhan biet moi vong bi ghi de neu tre nhieu vong DMA. */

#endif
