#include "STM32F334_I2C.h"
#include "STM32F334_CORE.h"
#include <stddef.h>

/* Doi chieu ST RM0364, I2C master va Table124:
 * https://www.st.com/resource/en/reference_manual/DM00093941.pdf
 * Bit positions: STMicroelectronics/cmsis-device-f3/Include/stm32f334x8.h.
 * ISR chi di chuyen byte va doi state; callback chi chay trong Process.
 */
#define I2C_EV_IRQ 31U
#define I2C_ER_IRQ 32U
#define I2C_CR1_PE (1U << 0)
#define I2C_CR1_TXIE (1U << 1)
#define I2C_CR1_RXIE (1U << 2)
#define I2C_CR1_NACKIE (1U << 4)
#define I2C_CR1_STOPIE (1U << 5)
#define I2C_CR1_TCIE (1U << 6)
#define I2C_CR1_ERRIE (1U << 7)
#define I2C_CR1_CONTROL (I2C_CR1_PE | I2C_CR1_NACKIE | I2C_CR1_STOPIE | I2C_CR1_ERRIE)
#define I2C_CR2_READ (1U << 10)
#define I2C_CR2_START (1U << 13)
#define I2C_CR2_STOP (1U << 14)
#define I2C_CR2_AUTOEND (1U << 25)
#define I2C_ISR_TXIS (1U << 1)
#define I2C_ISR_RXNE (1U << 2)
#define I2C_ISR_NACKF (1U << 4)
#define I2C_ISR_STOPF (1U << 5)
#define I2C_ISR_TC (1U << 6)
#define I2C_ISR_BERR (1U << 8)
#define I2C_ISR_ARLO (1U << 9)
#define I2C_ISR_OVR (1U << 10)
#define I2C_ISR_BUSY (1U << 15)
#define I2C_ERROR_FLAGS (I2C_ISR_BERR | I2C_ISR_ARLO | I2C_ISR_OVR)
#define I2C_CLEAR_FLAGS (I2C_ERROR_FLAGS | I2C_ISR_NACKF | I2C_ISR_STOPF)
#define I2C_ABORT_GRACE_MS 2U

_Static_assert(offsetof(I2C_RegDef_t, TIMINGR) == 0x10U, "I2C TIMINGR offset");
_Static_assert(offsetof(I2C_RegDef_t, ISR) == 0x18U, "I2C ISR offset");
_Static_assert(offsetof(I2C_RegDef_t, RXDR) == 0x24U, "I2C RXDR offset");

static I2C_Handle_t *owner; /* Chi registry so huu; moi state/buffer o handle. */

static bool owns_peripheral(const I2C_Handle_t *handle)
{
    return handle != NULL && owner == handle && handle->Initialized &&
           handle->pI2Cx == I2C1;
}

void I2C_PeriClockControl(I2C_RegDef_t *pI2Cx, uint8_t EnorDi)
{
    if (pI2Cx != I2C1) return;
    if (EnorDi == ENABLE) {
        RCC->APB1ENR |= 1U << 21;
        (void)RCC->APB1ENR;
    } else if (EnorDi == DISABLE) {
        RCC->APB1ENR &= ~(1U << 21);
    }
}

static void reset_peripheral(I2C_Handle_t *handle)
{
    /* Reset chi controller, KHONG tu toggle GPIO hay phat xung recovery len bus.
     * Neu slave giu SDA/SCL low, BUSY van can duoc xu ly nhu bus chua san sang.
     */
    handle->pI2Cx->CR1 = 0U;
    RCC->APB1RSTR |= 1U << 21;
    RCC->APB1RSTR &= ~(1U << 21);
    handle->pI2Cx->CR2 = 0U;
    handle->pI2Cx->OAR1 = 0U;
    handle->pI2Cx->OAR2 = 0U;
    handle->pI2Cx->TIMINGR = handle->ConfiguredTiming;
    handle->pI2Cx->TIMEOUTR = 0U;
    handle->pI2Cx->ICR = I2C_CLEAR_FLAGS;
    handle->pI2Cx->CR1 = I2C_CR1_PE; /* Analog filter ON, DNF=0. */
    NVIC_ClearPendingIRQ(I2C_EV_IRQ);
    NVIC_ClearPendingIRQ(I2C_ER_IRQ);
    handle->BusOwned = false;
}

bool I2C_Init(I2C_Handle_t *handle)
{
    if (handle == NULL || handle->pI2Cx != I2C1 ||
        handle->I2C_Config.I2C_KernelHz != 8000000U ||
        handle->I2C_Config.I2C_Timing == 0U ||
        (handle->I2C_Config.I2C_Timing & 0x0F000000U) != 0U ||
        handle->I2C_Config.I2C_IRQPriority > 15U) return false;
    uint32_t key = IRQ_Save();
    if (owner != NULL || (RCC->CFGR3 & (1U << 4)) != 0U ||
        (RCC->CR & (1U << 1)) == 0U) {
        IRQ_Restore(key);
        return false;
    }
    NVIC_DisableIRQ(I2C_EV_IRQ);
    NVIC_DisableIRQ(I2C_ER_IRQ);
    I2C_PeriClockControl(handle->pI2Cx, ENABLE);
    handle->ConfiguredTiming = handle->I2C_Config.I2C_Timing;
    handle->QueueHead = handle->QueueTail = handle->QueueCount = 0U;
    handle->TxCount = handle->RxCount = 0U;
    handle->State = I2C_STATE_IDLE;
    handle->Result = I2C_OK;
    handle->StartedMs = handle->AbortMs = 0U;
    handle->ProcessRunning = false;
    handle->Stats = (I2C_Stats){0};
    reset_peripheral(handle);
    owner = handle;
    handle->Initialized = true;
    NVIC_SetPriority(I2C_EV_IRQ, handle->I2C_Config.I2C_IRQPriority);
    NVIC_SetPriority(I2C_ER_IRQ, handle->I2C_Config.I2C_IRQPriority);
    NVIC_EnableIRQ(I2C_EV_IRQ);
    NVIC_EnableIRQ(I2C_ER_IRQ);
    IRQ_Restore(key);
    return true;
}

bool I2C_DeInit(I2C_Handle_t *handle)
{
    if (!owns_peripheral(handle)) return false;
    uint32_t key = IRQ_Save();
    if (handle->State != I2C_STATE_IDLE || handle->QueueCount != 0U ||
        handle->ProcessRunning) {
        IRQ_Restore(key);
        return false;
    }
    NVIC_DisableIRQ(I2C_EV_IRQ);
    NVIC_DisableIRQ(I2C_ER_IRQ);
    handle->pI2Cx->CR1 = 0U;
    handle->pI2Cx->CR2 = 0U;
    NVIC_ClearPendingIRQ(I2C_EV_IRQ);
    NVIC_ClearPendingIRQ(I2C_ER_IRQ);
    I2C_PeriClockControl(handle->pI2Cx, DISABLE);
    handle->Initialized = false;
    owner = NULL;
    IRQ_Restore(key);
    return true;
}

bool I2C_Submit(I2C_Handle_t *handle, const I2C_Transaction *transaction)
{
    if (!owns_peripheral(handle) || transaction == NULL ||
        transaction->address < 0x08U || transaction->address > 0x77U ||
        transaction->tx_length > 255U || transaction->rx_length > 255U ||
        (transaction->tx_length == 0U && transaction->rx_length == 0U) ||
        (transaction->tx_length != 0U && transaction->tx == NULL) ||
        (transaction->rx_length != 0U && transaction->rx == NULL) ||
        transaction->timeout_ms == 0U || transaction->timeout_ms > 0x7FFFFFFFU)
        return false;
    if (handle->QueueCount == I2C_TRANSACTION_QUEUE_SIZE) {
        ++handle->Stats.queue_full;
        return false;
    }
    handle->Queue[handle->QueueHead] = *transaction;
    handle->QueueHead = (uint8_t)((handle->QueueHead + 1U) % I2C_TRANSACTION_QUEUE_SIZE);
    ++handle->QueueCount;
    ++handle->Stats.submitted;
    return true;
}

static void begin_phase(I2C_Handle_t *handle, bool read)
{
    uint32_t length = read ? handle->Active.rx_length : handle->Active.tx_length;
    uint32_t control = I2C_CR1_CONTROL | (read ? I2C_CR1_RXIE : I2C_CR1_TXIE);
    uint32_t cr2 = ((uint32_t)handle->Active.address << 1) | (length << 16);
    if (read) cr2 |= I2C_CR2_READ;
    if (read || handle->Active.rx_length == 0U) cr2 |= I2C_CR2_AUTOEND;
    else control |= I2C_CR1_TCIE; /* Write prefix xong thi repeated START doc. */
    handle->State = read ? I2C_STATE_RX : I2C_STATE_TX;
    handle->BusOwned = true;
    handle->pI2Cx->CR1 = control;
    handle->pI2Cx->CR2 = cr2 | I2C_CR2_START;
}

static void fail_transfer(I2C_Handle_t *handle, I2C_Result result, uint32_t status)
{
    /* Loi lap lai khong duoc keo dai abort deadline vo han. ARLO van uu tien
     * vi no doi quyen so huu bus va cam ta gui STOP tiep.
     */
    if (handle->State == I2C_STATE_WAIT_STOP && result != I2C_ARBITRATION_LOST)
        return;
    handle->Result = result;
    handle->pI2Cx->CR1 = I2C_CR1_CONTROL; /* Tat TX/RX/TC de khong them byte. */
    if (result == I2C_ARBITRATION_LOST) {
        /* ARLO: ta KHONG con so huu bus. Khong STOP, khong pha master thang. */
        handle->BusOwned = false;
        handle->pI2Cx->CR1 = I2C_CR1_PE;
        handle->State = I2C_STATE_DONE;
    } else if (result == I2C_NACK ||
               (handle->BusOwned && (status & I2C_ISR_BUSY) != 0U)) {
        handle->AbortMs = Timebase_Millis();
        handle->State = I2C_STATE_WAIT_STOP;
        /* NACK tu dong sinh STOP, ke ca AUTOEND=0. Cac loi khac yeu cau STOP. */
        if (result != I2C_NACK)
            handle->pI2Cx->CR2 =
                (handle->pI2Cx->CR2 & ~I2C_CR2_START) | I2C_CR2_STOP;
    } else {
        reset_peripheral(handle); /* Huy START chua thuc hien, khong phat STOP. */
        ++handle->Stats.recoveries;
        handle->State = I2C_STATE_DONE;
    }
}

static void service_irq(I2C_Handle_t *handle)
{
    uint32_t status = handle->pI2Cx->ISR;
    if (handle->State == I2C_STATE_TX || handle->State == I2C_STATE_RX ||
        handle->State == I2C_STATE_WAIT_STOP) {
        if ((status & I2C_ERROR_FLAGS) != 0U) {
            handle->pI2Cx->ICR = status & I2C_ERROR_FLAGS;
            I2C_Result result = (status & I2C_ISR_ARLO) ? I2C_ARBITRATION_LOST :
                                (status & I2C_ISR_BERR) ? I2C_BUS_ERROR : I2C_OVERRUN;
            fail_transfer(handle, result, status);
        } else if ((status & I2C_ISR_NACKF) != 0U) {
            handle->pI2Cx->ICR = I2C_ISR_NACKF;
            if (handle->State != I2C_STATE_WAIT_STOP)
                fail_transfer(handle, I2C_NACK, status);
        }
        if (handle->State == I2C_STATE_RX && (status & I2C_ISR_RXNE) != 0U) {
            uint8_t byte = (uint8_t)handle->pI2Cx->RXDR;
            if (handle->RxCount < handle->Active.rx_length)
                handle->Active.rx[handle->RxCount++] = byte;
            else fail_transfer(handle, I2C_OVERRUN, status);
            if (handle->RxCount == handle->Active.rx_length)
                handle->pI2Cx->CR1 &= ~I2C_CR1_RXIE;
        }
        if (handle->State == I2C_STATE_TX && (status & I2C_ISR_TXIS) != 0U &&
            (status & I2C_ISR_STOPF) == 0U) {
            if (handle->TxCount < handle->Active.tx_length)
                handle->pI2Cx->TXDR = handle->Active.tx[handle->TxCount++];
            else fail_transfer(handle, I2C_OVERRUN, status);
            if (handle->TxCount == handle->Active.tx_length)
                handle->pI2Cx->CR1 &= ~I2C_CR1_TXIE;
        }
        if (handle->State == I2C_STATE_TX && (status & I2C_ISR_TC) != 0U &&
            (status & I2C_ISR_STOPF) == 0U) {
            if (handle->TxCount == handle->Active.tx_length &&
                handle->Active.rx_length != 0U)
                begin_phase(handle, true); /* Khong STOP giua register addr va RX. */
            else fail_transfer(handle, I2C_BUS_ERROR, status);
        }
        if ((status & I2C_ISR_STOPF) != 0U) {
            handle->pI2Cx->ICR = I2C_ISR_STOPF;
            if (handle->Result == I2C_OK &&
                (handle->TxCount != handle->Active.tx_length ||
                 handle->RxCount != handle->Active.rx_length))
                handle->Result = I2C_BUS_ERROR; /* STOP som: khong bao thanh cong. */
            handle->pI2Cx->CR1 = I2C_CR1_PE;
            handle->BusOwned = false;
            handle->State = I2C_STATE_DONE;
        }
    }
}

static void count_result(I2C_Handle_t *handle)
{
    switch (handle->Result) {
    case I2C_OK: ++handle->Stats.completed; break;
    case I2C_NACK: ++handle->Stats.nacks; break;
    case I2C_TIMEOUT: ++handle->Stats.timeouts; break;
    case I2C_BUS_ERROR: ++handle->Stats.bus_errors; break;
    case I2C_ARBITRATION_LOST: ++handle->Stats.arbitration_lost; break;
    case I2C_OVERRUN: ++handle->Stats.overruns; break;
    }
}

void I2C_Process(I2C_Handle_t *handle)
{
    if (!owns_peripheral(handle) || handle->ProcessRunning) return;
    handle->ProcessRunning = true;
    I2C_Callback callback = NULL;
    void *context = NULL;
    I2C_Result result = I2C_OK;
    uint32_t key = IRQ_Save();
    service_irq(handle); /* Nhan STOP/error da toi truoc khi xet timeout. */
    uint32_t now = Timebase_Millis();
    if (handle->State == I2C_STATE_WAIT_BUS) {
        if ((uint32_t)(now - handle->StartedMs) >= handle->Active.timeout_ms) {
            handle->Result = I2C_TIMEOUT; /* Bus ngoai ban: khong STOP/reset no. */
            handle->State = I2C_STATE_DONE;
        } else if ((handle->pI2Cx->ISR & I2C_ISR_BUSY) == 0U) {
            reset_peripheral(handle);
            begin_phase(handle, handle->Active.tx_length == 0U);
        }
    } else if (handle->State == I2C_STATE_TX || handle->State == I2C_STATE_RX) {
        if ((uint32_t)(now - handle->StartedMs) >= handle->Active.timeout_ms)
            fail_transfer(handle, I2C_TIMEOUT, handle->pI2Cx->ISR);
    } else if (handle->State == I2C_STATE_WAIT_STOP &&
               (uint32_t)(now - handle->AbortMs) >= I2C_ABORT_GRACE_MS) {
        reset_peripheral(handle); /* STOP khong hoan tat: release controller. */
        ++handle->Stats.recoveries;
        handle->State = I2C_STATE_DONE;
    }
    if (handle->State == I2C_STATE_DONE) {
        callback = handle->Active.callback;
        context = handle->Active.context;
        result = handle->Result;
        count_result(handle);
        handle->State = I2C_STATE_IDLE;
    }
    if (handle->State == I2C_STATE_IDLE && handle->QueueCount != 0U) {
        handle->Active = handle->Queue[handle->QueueTail];
        handle->QueueTail = (uint8_t)((handle->QueueTail + 1U) % I2C_TRANSACTION_QUEUE_SIZE);
        --handle->QueueCount;
        handle->TxCount = handle->RxCount = 0U;
        handle->Result = I2C_OK;
        handle->StartedMs = now;
        handle->State = I2C_STATE_WAIT_BUS;
        if ((handle->pI2Cx->ISR & I2C_ISR_BUSY) == 0U) {
            reset_peripheral(handle);
            begin_phase(handle, handle->Active.tx_length == 0U);
        } else ++handle->Stats.busy_waits;
    }
    IRQ_Restore(key);
    if (callback != NULL) callback(result, context);
    handle->ProcessRunning = false;
}

bool I2C_Busy(I2C_Handle_t *handle)
{
    if (!owns_peripheral(handle)) return false;
    uint32_t key = IRQ_Save();
    bool busy = handle->State != I2C_STATE_IDLE || handle->QueueCount != 0U;
    IRQ_Restore(key);
    return busy;
}

void I2C_GetStats(I2C_Handle_t *handle, I2C_Stats *stats)
{
    if (!owns_peripheral(handle) || stats == NULL) return;
    uint32_t key = IRQ_Save();
    *stats = handle->Stats;
    IRQ_Restore(key);
}

void I2C_EVIRQHandling(I2C_Handle_t *handle)
{
    if (!owns_peripheral(handle)) return;
    uint32_t key = IRQ_Save();
    service_irq(handle);
    IRQ_Restore(key);
}

void I2C_ERIRQHandling(I2C_Handle_t *handle)
{
    I2C_EVIRQHandling(handle); /* Chung state va error handling, khong callback. */
}
