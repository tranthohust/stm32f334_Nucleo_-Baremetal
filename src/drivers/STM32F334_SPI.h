#ifndef STM32F334_SPI_H
#define STM32F334_SPI_H

#include "STM32F334.h"
#include <stdbool.h>
#include <stdint.h>

/* Cung phong cach GPIO: Config -> Handle -> Init -> IRQHandling(handle).
 * Backend dung ngat; hien ho tro SPI1 master, 8-bit, MSB-first.
 * GPIO alternate function va CS do ung dung cau hinh, khong hard-code trong driver.
 */
#define SPI_DEVICE_MODE_MASTER 1U
#define SPI_DFF_8BITS 0U
#define SPI_CPOL_LOW 0U
#define SPI_CPOL_HIGH 1U
#define SPI_CPHA_FIRST_EDGE 0U
#define SPI_CPHA_SECOND_EDGE 1U
#define SPI_SCLK_SPEED_DIV2 0U
#define SPI_SCLK_SPEED_DIV4 1U
#define SPI_SCLK_SPEED_DIV8 2U
#define SPI_SCLK_SPEED_DIV16 3U
#define SPI_SCLK_SPEED_DIV32 4U
#define SPI_SCLK_SPEED_DIV64 5U
#define SPI_SCLK_SPEED_DIV128 6U
#define SPI_SCLK_SPEED_DIV256 7U
#define SPI_TRANSACTION_QUEUE_SIZE 4U

typedef struct {
    uint8_t SPI_DeviceMode;    /* Hien chi SPI_DEVICE_MODE_MASTER. */
    uint8_t SPI_DataFrameSize; /* Hien chi SPI_DFF_8BITS; khong la raw DS bits. */
    uint8_t SPI_CPOL;
    uint8_t SPI_CPHA;
    uint8_t SPI_SclkSpeed;     /* BR encoding: PCLK2 / (2 << SPI_SclkSpeed). */
    uint8_t SPI_IRQPriority;   /* 0..15; 0 la cao nhat. */
} SPI_Config_t;
typedef enum {
    SPI_OK = 0,
    SPI_TIMEOUT,
    SPI_HW_ERROR
} SPI_Result;

typedef void (*SPI_Callback)(SPI_Result result, void *context);

typedef struct {
    GPIO_RegDef_t *cs_port;
    uint8_t cs_pin;
    const uint8_t *tx;  /* NULL: gui 0xFF de tao clock cho viec doc. */
    uint8_t *rx;        /* NULL: van doc DR nhung bo du lieu nhan. */
    uint16_t length;    /* So byte clock; ca TX va RX deu co length byte. */
    uint32_t timeout_ms; /* Tinh tu luc CS xuong thap, khong tinh thoi gian queue. */
    SPI_Callback callback;
    void *context;
} SPI_Transaction;

typedef enum {
    SPI_STATE_IDLE,
    SPI_STATE_TRANSFER,
    SPI_STATE_WAIT_END,
    SPI_STATE_ERROR
} SPI_State;

typedef struct {
    SPI_RegDef_t *pSPIx;
    SPI_Config_t SPI_Config;

    /* Runtime thuoc HANDLE nay, khong nam trong static globals cua driver.
     * Ung dung chi gan pSPIx/SPI_Config truoc Init, khong tu sua cac field sau.
     * Handle phai la static/global, ton tai den khi DeInit thanh cong.
     */
    SPI_Transaction Queue[SPI_TRANSACTION_QUEUE_SIZE];
    uint8_t QueueHead, QueueTail, QueueCount;
    SPI_Transaction Active;
    volatile SPI_State State;
    volatile uint16_t TxCount, RxCount;
    uint32_t StartedMs;
    uint32_t ConfiguredCR1; /* Snapshot cau hinh da nap luc Init. */
    bool Initialized;
    bool ProcessRunning;
} SPI_Handle_t;

/* Khoi tao handle bang {0}, gan pSPIx va SPI_Config, roi Init.
 * API chi goi tu main, tru IRQHandling chi duoc vector ISR goi.
 * Init tu enable peripheral clock; GPIO/CS phai duoc app cau hinh truoc.
 * Moi peripheral chi duoc mot handle so huu; Init lai can DeInit truoc.
 * Muon doi mode/speed: doi khi idle, DeInit, sua Config, Init lai.
 */
/* Helper thanh ghi giong GPIO_PeriClockControl; khong tat clock khi dang gui. */
void SPI_PeriClockControl(SPI_RegDef_t *pSPIx, uint8_t EnorDi);
bool SPI_Init(SPI_Handle_t *pSPIHandle);

/* DeInit chi thanh cong khi idle, queue rong va khong trong callback/Process.
 * Khong tu huy callback hay buffer dang duoc driver so huu.
 */
bool SPI_DeInit(SPI_Handle_t *pSPIHandle);

/* Queue sao chep descriptor, KHONG sao chep tx/rx/context.
 * Cac buffer phai con song va khong bi caller sua den khi callback duoc goi.
 * Co 4 cho doi + 1 transaction dang chay. false: invalid/chua init/queue day.
 * Chi goi API tu main; callback duoc phep Submit(), khong goi Process() de quy.
 */
bool SPI_Submit(SPI_Handle_t *pSPIHandle, const SPI_Transaction *transaction);
void SPI_Process(SPI_Handle_t *pSPIHandle);
bool SPI_Busy(SPI_Handle_t *pSPIHandle);

/* App dinh nghia SPI1_IRQHandler va forward handle da Init vao day. */
void SPI_IRQHandling(SPI_Handle_t *pSPIHandle);

#endif
