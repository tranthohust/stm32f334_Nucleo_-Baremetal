#include "STM32F334_SPI.h"
#include "STM32F334_CORE.h"
#include <stddef.h>

/* Doi chieu RM0364, chuong SPI; bit positions tu header chinh thuc cua ST:
 * https://github.com/STMicroelectronics/cmsis-device-f3/blob/master/Include/stm32f334x8.h
 * Khong copy HAL: driver nay viet truc tiep thanh ghi de de theo doi.
 */
#define SPI1_IRQ_NUMBER       35U
#define SPI_CR1_CPHA          (1U << 0)
#define SPI_CR1_CPOL          (1U << 1)
#define SPI_CR1_MSTR          (1U << 2)
#define SPI_CR1_SPE           (1U << 6)
#define SPI_CR1_SSI           (1U << 8)
#define SPI_CR1_SSM           (1U << 9)
#define SPI_CR2_ERRIE         (1U << 5)
#define SPI_CR2_RXNEIE        (1U << 6)
#define SPI_CR2_TXEIE         (1U << 7)
#define SPI_CR2_BASE          ((7U << 8) | (1U << 12)) /* DS=8-bit, FRXTH=8-bit. */
#define SPI_SR_RXNE          (1U << 0)
#define SPI_SR_TXE           (1U << 1)
#define SPI_SR_CRCERR        (1U << 4)
#define SPI_SR_MODF          (1U << 5)
#define SPI_SR_OVR           (1U << 6)
#define SPI_SR_BSY           (1U << 7)
#define SPI_SR_FRE           (1U << 8)
#define SPI_SR_FTLVL         (3U << 11)
#define SPI_SR_ERRORS        (SPI_SR_CRCERR | SPI_SR_MODF | SPI_SR_OVR | SPI_SR_FRE)

_Static_assert(offsetof(SPI_RegDef_t, DR) == 0x0CU, "SPI DR offset");

/* Chi registry so huu la static; queue/state thuc su nam trong handle cua app. */
static SPI_Handle_t *owner;

static bool owns_peripheral(const SPI_Handle_t *handle)
{
    /* So sanh owner TRUOC khi doc runtime: handle la {0}/chua Init khong the
     * cham thanh ghi, handle khac cung khong the chiem peripheral dang dung.
     */
    return handle != NULL && owner == handle && handle->Initialized &&
           handle->pSPIx == SPI1;
}

static bool valid_cs(GPIO_RegDef_t *port, uint8_t pin)
{
    return pin < 16U && (port == GPIOA || port == GPIOB || port == GPIOC ||
                        port == GPIOD || port == GPIOF);
}

static void chip_select(SPI_Handle_t *handle, bool asserted)
{
    /* BSRR la write-only set/reset: khong read-modify-write ODR. */
    handle->Active.cs_port->BSRR =
        1U << (handle->Active.cs_pin + (asserted ? 16U : 0U));
}

static void reset_peripheral(SPI_Handle_t *handle)
{
    /* Reset xoa FIFO va co loi, ke ca khi transaction bi timeout.
     * Abort co the cat ngan byte tren day; callback bao that bai cho caller.
     */
    handle->pSPIx->CR2 = 0U;
    RCC->APB2RSTR |= 1U << 12;
    RCC->APB2RSTR &= ~(1U << 12);
    handle->pSPIx->CR2 = SPI_CR2_BASE;
    NVIC_ClearPendingIRQ(SPI1_IRQ_NUMBER);
}

static void start_next(SPI_Handle_t *handle)
{
    /* Caller da khoa IRQ. Queue chi duoc main thay doi; ISR chi thay doi phase
     * va tien do active. Khong can malloc hay queue generic de hoc buoc nay.
     */
    handle->Active = handle->Queue[handle->QueueTail];
    handle->QueueTail = (uint8_t)((handle->QueueTail + 1U) % SPI_TRANSACTION_QUEUE_SIZE);
    --handle->QueueCount;
    handle->TxCount = 0U;
    handle->RxCount = 0U;
    reset_peripheral(handle);

    /* CPOL/CPHA/BR chi sua khi SPE=0, truoc khi assert CS.
     * SSI=1 voi SSM=1 giu NSS noi bo cao, tranh MODF trong master mode.
     */
    handle->pSPIx->CR1 = handle->ConfiguredCR1;
    handle->StartedMs = Timebase_Millis();
    handle->State = SPI_STATE_TRANSFER;
    /* Dua SCK ve muc idle cua mode moi truoc khi thiet bi duoc chon. */
    handle->pSPIx->CR1 |= SPI_CR1_SPE;
    chip_select(handle, true);
    handle->pSPIx->CR2 = SPI_CR2_BASE | SPI_CR2_RXNEIE | SPI_CR2_TXEIE | SPI_CR2_ERRIE;
}

void SPI_PeriClockControl(SPI_RegDef_t *pSPIx, uint8_t EnorDi)
{
    if (pSPIx != SPI1) return;
    if (EnorDi == ENABLE) {
        RCC->APB2ENR |= 1U << 12;
        (void)RCC->APB2ENR;
    } else if (EnorDi == DISABLE) {
        RCC->APB2ENR &= ~(1U << 12);
    }
}

bool SPI_Init(SPI_Handle_t *handle)
{
    if (handle == NULL || handle->pSPIx != SPI1 ||
        handle->SPI_Config.SPI_DeviceMode != SPI_DEVICE_MODE_MASTER ||
        handle->SPI_Config.SPI_DataFrameSize != SPI_DFF_8BITS ||
        handle->SPI_Config.SPI_CPOL > SPI_CPOL_HIGH ||
        handle->SPI_Config.SPI_CPHA > SPI_CPHA_SECOND_EDGE ||
        handle->SPI_Config.SPI_SclkSpeed > SPI_SCLK_SPEED_DIV256 ||
        handle->SPI_Config.SPI_IRQPriority > 15U) {
        return false;
    }
    uint32_t saved = IRQ_Save();
    if (owner != NULL) {
        IRQ_Restore(saved);
        return false;
    }
    NVIC_DisableIRQ(SPI1_IRQ_NUMBER);
    SPI_PeriClockControl(handle->pSPIx, ENABLE);
    handle->QueueHead = 0U;
    handle->QueueTail = 0U;
    handle->QueueCount = 0U;
    handle->TxCount = 0U;
    handle->RxCount = 0U;
    handle->State = SPI_STATE_IDLE;
    handle->ProcessRunning = false;
    handle->StartedMs = 0U;
    handle->ConfiguredCR1 = SPI_CR1_MSTR | SPI_CR1_SSM | SPI_CR1_SSI |
                           ((uint32_t)handle->SPI_Config.SPI_SclkSpeed << 3) |
                           (handle->SPI_Config.SPI_CPHA ? SPI_CR1_CPHA : 0U) |
                           (handle->SPI_Config.SPI_CPOL ? SPI_CR1_CPOL : 0U);
    reset_peripheral(handle);
    handle->pSPIx->CR1 = handle->ConfiguredCR1;
    owner = handle;
    handle->Initialized = true;
    NVIC_SetPriority(SPI1_IRQ_NUMBER, handle->SPI_Config.SPI_IRQPriority);
    NVIC_EnableIRQ(SPI1_IRQ_NUMBER);
    IRQ_Restore(saved);
    return true;
}

bool SPI_DeInit(SPI_Handle_t *handle)
{
    if (!owns_peripheral(handle)) return false;
    uint32_t saved = IRQ_Save();
    if (handle->State != SPI_STATE_IDLE || handle->QueueCount != 0U ||
        handle->ProcessRunning) {
        IRQ_Restore(saved);
        return false;
    }
    NVIC_DisableIRQ(SPI1_IRQ_NUMBER);
    reset_peripheral(handle);
    handle->pSPIx->CR2 = 0U;
    handle->pSPIx->CR1 = 0U;
    SPI_PeriClockControl(handle->pSPIx, DISABLE);
    handle->Initialized = false;
    owner = NULL;
    IRQ_Restore(saved);
    return true;
}

bool SPI_Submit(SPI_Handle_t *handle, const SPI_Transaction *transaction)
{
    if (!owns_peripheral(handle) || transaction == NULL ||
        !valid_cs(transaction->cs_port, transaction->cs_pin) ||
        transaction->length == 0U || transaction->timeout_ms == 0U ||
        transaction->timeout_ms > 0x7FFFFFFFU ||
        handle->QueueCount == SPI_TRANSACTION_QUEUE_SIZE) {
        return false;
    }
    /* Chi sao chep descriptor. CS da o high tu Init/cau hinh board;
     * khong cham CS o day vi co the transaction truoc dang dung cung CS.
     */
    handle->Queue[handle->QueueHead] = *transaction;
    handle->QueueHead = (uint8_t)((handle->QueueHead + 1U) % SPI_TRANSACTION_QUEUE_SIZE);
    ++handle->QueueCount;
    return true;
}

void SPI_Process(SPI_Handle_t *handle)
{
    if (!owns_peripheral(handle) || handle->ProcessRunning) {
        return;
    }
    handle->ProcessRunning = true;
    SPI_Callback callback = NULL;
    void *context = NULL;
    SPI_Result result = SPI_OK;
    bool finished = false;

    uint32_t saved = IRQ_Save();
    if (handle->State != SPI_STATE_IDLE) {
        uint32_t status = handle->pSPIx->SR;
        if (handle->State == SPI_STATE_ERROR || (status & SPI_SR_ERRORS) != 0U) {
            result = SPI_HW_ERROR;
            finished = true;
        } else if ((uint32_t)(Timebase_Millis() - handle->StartedMs) >=
                   handle->Active.timeout_ms) {
            result = SPI_TIMEOUT;
            finished = true;
        } else if (handle->State == SPI_STATE_WAIT_END &&
                   (status & (SPI_SR_BSY | SPI_SR_FTLVL)) == 0U) {
            /* Da doc du RX va TX FIFO rong, BSY=0: byte cuoi da ra khoi bus.
             * Khong spin-wait trong ISR; lan main sau se kiem tra tiep.
             */
            finished = true;
        }
    }
    if (finished) {
        handle->pSPIx->CR2 = SPI_CR2_BASE; /* Ngung IRQ truoc khi doi active. */
        if (result != SPI_OK) {
            reset_peripheral(handle);
        } else {
            handle->pSPIx->CR1 &= ~SPI_CR1_SPE;
        }
        chip_select(handle, false);
        callback = handle->Active.callback;
        context = handle->Active.context;
        handle->State = SPI_STATE_IDLE;
    }
    if (handle->State == SPI_STATE_IDLE && handle->QueueCount != 0U) {
        start_next(handle);
    }
    IRQ_Restore(saved);

    /* Callback chay trong main voi IRQ da khoi phuc, co the Submit goi moi.
     * Tuyet doi khong goi callback/printf/parser trong SPI_IRQHandling.
     */
    if (callback != NULL) {
        callback(result, context);
    }
    handle->ProcessRunning = false;
}

bool SPI_Busy(SPI_Handle_t *handle)
{
    if (!owns_peripheral(handle)) return false;
    uint32_t saved = IRQ_Save();
    bool busy = handle->State != SPI_STATE_IDLE || handle->QueueCount != 0U;
    IRQ_Restore(saved);
    return busy;
}

void SPI_IRQHandling(SPI_Handle_t *handle)
{
    if (!owns_peripheral(handle)) return;
    if (handle->State != SPI_STATE_TRANSFER) {
        handle->pSPIx->CR2 = SPI_CR2_BASE;
        return;
    }
    uint32_t status = handle->pSPIx->SR;
    if ((status & SPI_SR_ERRORS) != 0U) {
        handle->pSPIx->CR2 = SPI_CR2_BASE;
        handle->State = SPI_STATE_ERROR;
        return;
    }

    /* Chi cho 1 byte dang cho RX tai moi thoi diem. De hoc hon cach nap day
     * FIFO; doi lai throughput thap hon, phu hop clock ban dau PCLK2/16.
     * Moi lan vao ISR toi da doc 1 byte + gui 1 byte: thoi gian xu ly huu han.
     */
    if ((status & SPI_SR_RXNE) != 0U) {
        uint8_t value = *(volatile uint8_t *)&handle->pSPIx->DR;
        if (handle->RxCount >= handle->TxCount ||
            handle->RxCount >= handle->Active.length) {
            handle->pSPIx->CR2 = SPI_CR2_BASE;
            handle->State = SPI_STATE_ERROR;
            return;
        }
        if (handle->Active.rx != NULL) {
            handle->Active.rx[handle->RxCount] = value;
        }
        ++handle->RxCount;
        if (handle->RxCount == handle->Active.length) {
            handle->pSPIx->CR2 = SPI_CR2_BASE;
            handle->State = SPI_STATE_WAIT_END;
            return;
        }
        handle->pSPIx->CR2 |= SPI_CR2_TXEIE;
    }
    if ((handle->pSPIx->SR & SPI_SR_TXE) != 0U &&
        (handle->pSPIx->CR2 & SPI_CR2_TXEIE) != 0U) {
        uint8_t value = handle->Active.tx == NULL ? 0xFFU :
                        handle->Active.tx[handle->TxCount];
        /* Phai truy cap 8-bit: ghi DR 16/32-bit voi DS=8-bit co the pack 2 byte. */
        *(volatile uint8_t *)&handle->pSPIx->DR = value;
        ++handle->TxCount;
        /* TXE co the bat lai truoc khi RX xong; tat de tranh IRQ storm. */
        handle->pSPIx->CR2 &= ~SPI_CR2_TXEIE;
    }
}
