#ifndef STM32F334_CAN_H
#define STM32F334_CAN_H

#include "STM32F334.h"
#include <stdbool.h>
#include <stdint.h>

#define CAN_RX_QUEUE_SIZE 8U
#define CAN_TX_QUEUE_SIZE 8U
#define CAN_RESULT_QUEUE_SIZE 8U
#define CAN_MODE_NORMAL 0U
#define CAN_MODE_SILENT_LOOPBACK 1U

/* CAN co bien khung san: buffer chua ca frame, khong phai byte stream nhu UART. */
typedef struct {
    uint32_t id;       /* standard: 0..0x7FF; extended: 0..0x1FFFFFFF */
    uint8_t dlc;       /* Classical CAN: 0..8; remote frame van co DLC. */
    bool extended;
    bool remote;
    uint8_t data[8];   /* Khong co payload khi remote=true. */
} CAN_Frame;

typedef enum {
    CAN_TX_OK = 0,
    CAN_TX_ERROR,
    CAN_TX_ARBITRATION_LOST,
    CAN_TX_TIMEOUT,
    CAN_TX_BUS_OFF,
    CAN_TX_ABORTED
} CAN_TxStatus;

typedef struct {
    CAN_Frame frame;
    CAN_TxStatus status;
    uint32_t error_status; /* Snapshot CAN_ESR: LEC, TEC, REC va bus-off. */
} CAN_TxResult;

typedef struct {
    uint32_t rx_frames;
    uint32_t rx_queue_overflow;
    uint32_t rx_fifo_overflow;
    uint32_t rx_invalid_dlc;
    uint32_t tx_queued;
    uint32_t tx_queue_full;
    uint32_t tx_completed;
    uint32_t tx_failed;
    uint32_t tx_timeouts;
    uint32_t tx_result_overflow;
    uint32_t error_interrupts;
    uint32_t bus_off_events;
    uint32_t bus_off_recoveries;
    uint32_t init_timeouts;
    uint32_t last_error_status;
    uint32_t bit_timing;
    bool bus_off;
} CAN_Stats;

/* Config la cau hinh nguoi dung; handle la peripheral + cau hinh + trang thai.
 * CAN_PCLKHz la APB1, khong phai clock CPU; 8MHz ho tro 125k/250k/500k.
 */
typedef struct {
    uint32_t CAN_PCLKHz;
    uint32_t CAN_BitRate;
    uint8_t CAN_Mode;
    uint8_t CAN_IRQPriority;  /* 0..15, so nho uu tien cao. */
    uint32_t CAN_TxTimeoutMs; /* 1..0x7FFFFFFF; can SysTick 1ms dang chay. */
} CAN_Config_t;

typedef struct {
    CAN_Frame frame;
    uint32_t started_ms;
    CAN_TxStatus abort_reason;
    bool active;
    bool abort_requested;
} CAN_InFlight;

typedef struct {
    CAN_RegDef_t *pCANx;
    CAN_Config_t CAN_Config;

    /* Driver so huu cac truong ben duoi; ung dung khong sua khi dang chay.
     * Handle phai con song den khi CAN_DeInit thanh cong (nen dung static).
     */
    CAN_Frame rx_queue[CAN_RX_QUEUE_SIZE];
    CAN_Frame tx_queue[CAN_TX_QUEUE_SIZE];
    CAN_TxResult result_queue[CAN_RESULT_QUEUE_SIZE];
    CAN_InFlight inflight[3];
    uint8_t rx_head, rx_tail, rx_count;
    uint8_t tx_head, tx_tail, tx_count;
    uint8_t result_head, result_tail, result_count;
    CAN_Stats counters;
    bool initialized;
} CAN_Handle_t;

/* Chi ho tro CAN1. Khong sua pCANx/CAN_Config khi handle dang hoat dong.
 * Moi peripheral chi bind duoc mot handle; phai DeInit
 * truoc khi Init lai/doi handle. Init loi se tu tha binding cho lan thu sau.
 * Driver khong setup GPIO: board dung GPIO_Init de chon PA11/PA12 AF9.
 * Silent-loopback khong can transceiver; normal mode can bus da dau noi.
 * Filter demo nhan tat ca vao FIFO0; RX1 handling van duoc ho tro.
 */
void CAN_PeriClockControl(CAN_RegDef_t *pCANx, uint8_t EnorDi);
bool CAN_Init(CAN_Handle_t *pCANHandle);
/* Chi DeInit khi khong con TX active/queued; RX/result chua doc se bi bo. */
bool CAN_DeInit(CAN_Handle_t *pCANHandle);

/* Send copy frame vao queue; true chi bao da nhan yeu cau, chua bao phat xong.
 * Driver tu retransmit khi loi/arbitration loss; qua timeout se yeu cau abort.
 * Can SysTick 1ms dang chay va CAN_Process() deu dan de timeout hoat dong.
 * TX result queue can duoc doc deu dan; tran thi bo result moi va tang counter.
 */
bool CAN_Send(CAN_Handle_t *pCANHandle, const CAN_Frame *frame);
bool CAN_Receive(CAN_Handle_t *pCANHandle, CAN_Frame *frame);
bool CAN_GetTxResult(CAN_Handle_t *pCANHandle, CAN_TxResult *result);
void CAN_Process(CAN_Handle_t *pCANHandle);
void CAN_GetStats(CAN_Handle_t *pCANHandle, CAN_Stats *stats);

/* Vector wrapper cua ung dung truyen dung handle vao ham IRQHandling.
 * Driver khong dinh nghia CAN_*_IRQHandler, khong goi callback ung dung trong ISR.
 */
void CAN_TXIRQHandling(CAN_Handle_t *pCANHandle);
void CAN_RX0IRQHandling(CAN_Handle_t *pCANHandle);
void CAN_RX1IRQHandling(CAN_Handle_t *pCANHandle);
void CAN_SCEIRQHandling(CAN_Handle_t *pCANHandle);

#endif
