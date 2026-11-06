#ifndef STM32F334_I2C_H
#define STM32F334_I2C_H

#include "STM32F334.h"
#include <stdbool.h>
#include <stdint.h>

#define I2C_TRANSACTION_QUEUE_SIZE 4U
/* RM0364 Table 124: kernel HSI8, analog filter ON, digital filter 0.
 * PRESC=1, SCLDEL=4, SDADEL=2, SCLH=15, SCLL=19: xap xi 100kHz.
 * Toc do thuc te con phu thuoc rise/fall time va pull-up tren board.
 */
#define I2C_TIMING_100KHZ_HSI8 0x10420F13U

typedef struct {
    uint32_t I2C_KernelHz;  /* Bai hoc nay chi ho tro HSI8 = 8000000. */
    uint32_t I2C_Timing;    /* TIMINGR; reserved bits 27:24 phai bang 0. */
    uint8_t I2C_IRQPriority;
} I2C_Config_t;

typedef enum {
    I2C_OK = 0,
    I2C_NACK,
    I2C_TIMEOUT,
    I2C_BUS_ERROR,
    I2C_ARBITRATION_LOST,
    I2C_OVERRUN
} I2C_Result;

typedef void (*I2C_Callback)(I2C_Result result, void *context);

typedef struct {
    uint8_t address; /* 0x08..0x77, CHUA dich trai; driver gan SADD[7:1]. */
    const uint8_t *tx;
    uint8_t *rx;
    uint16_t tx_length; /* Moi phase 0..255 byte; tong hai phase phai >0. */
    uint16_t rx_length;
    uint32_t timeout_ms; /* Tinh tu khi lay khoi queue, ke ca cho BUSY. */
    I2C_Callback callback;
    void *context;
} I2C_Transaction;

typedef struct {
    uint32_t submitted, completed, queue_full;
    uint32_t nacks, timeouts, bus_errors, arbitration_lost, overruns;
    uint32_t recoveries, busy_waits;
} I2C_Stats;

typedef enum {
    I2C_STATE_IDLE,
    I2C_STATE_WAIT_BUS,
    I2C_STATE_TX,
    I2C_STATE_RX,
    I2C_STATE_WAIT_STOP,
    I2C_STATE_DONE
} I2C_State;

typedef struct {
    I2C_RegDef_t *pI2Cx;
    I2C_Config_t I2C_Config;
    /* Runtime do driver so huu. Handle phai ton tai den DeInit (static/global). */
    I2C_Transaction Queue[I2C_TRANSACTION_QUEUE_SIZE];
    uint8_t QueueHead, QueueTail, QueueCount;
    I2C_Transaction Active;
    volatile I2C_State State;
    volatile uint16_t TxCount, RxCount;
    volatile I2C_Result Result;
    uint32_t StartedMs, AbortMs, ConfiguredTiming;
    volatile bool BusOwned;
    bool Initialized, ProcessRunning;
    I2C_Stats Stats;
} I2C_Handle_t;

/* Style giong GPIO: {0} -> pI2Cx/Config -> Init. Hien chi I2C1 master 7-bit.
 * Driver khong cau hinh GPIO: app chon AF/open-drain, can pull-up ngoai.
 * Init yeu cau RCC I2C1SW=0 va HSI ready; khong tu doi clock mux.
 * Goi API tu MOT main/task so huu; EV/ER IRQ chi forward handle vao handler.
 * Khong task-safe: RTOS task khac gui request qua queue toi task so huu.
 */
/* Helper low-level: khong tat clock khi dang giao tiep. */
void I2C_PeriClockControl(I2C_RegDef_t *pI2Cx, uint8_t EnorDi);
bool I2C_Init(I2C_Handle_t *handle);
bool I2C_DeInit(I2C_Handle_t *handle); /* Chi idle, queue rong, ngoai callback. */

/* Submit copy descriptor, KHONG copy tx/rx/context. Buffer can song va khong
 * bi caller sua den callback trong Process. TX+RX tu dong repeated START.
 * Khong ho tro address-only probe (ca hai length=0), 10-bit hay RELOAD >255.
 * Khong tu retry khi NACK/ARLO; callback de ung dung quyet dinh.
 */
bool I2C_Submit(I2C_Handle_t *handle, const I2C_Transaction *transaction);
void I2C_Process(I2C_Handle_t *handle);
bool I2C_Busy(I2C_Handle_t *handle); /* Co transaction active/queued cua handle. */
void I2C_GetStats(I2C_Handle_t *handle, I2C_Stats *stats);
void I2C_EVIRQHandling(I2C_Handle_t *handle);
void I2C_ERIRQHandling(I2C_Handle_t *handle);

#endif
