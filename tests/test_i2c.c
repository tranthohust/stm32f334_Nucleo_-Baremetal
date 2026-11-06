/* Actual driver with RAM register mocks. Tests inject hardware snapshots;
 * RXDR/TXDR side effects and ICR W1C do not happen automatically in RAM.
 */
#include "../src/drivers/STM32F334_I2C.h"
#include "../src/drivers/STM32F334_CORE.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

static I2C_RegDef_t mock_i2c;
static RCC_RegDef_t mock_rcc;
static uint32_t mock_ms, mock_mask;
static unsigned nvic_writes;
static uint8_t irq_priority[2];
static bool irq_enabled[2];
#undef I2C1
#undef RCC
#define I2C1 (&mock_i2c)
#define RCC (&mock_rcc)

uint32_t IRQ_Save(void) { uint32_t saved = mock_mask; mock_mask = 1U; return saved; }
void IRQ_Restore(uint32_t saved) { mock_mask = saved; }
void NVIC_EnableIRQ(uint8_t irq)
{
    assert(irq == 31U || irq == 32U);
    irq_enabled[irq - 31U] = true; ++nvic_writes;
}
void NVIC_DisableIRQ(uint8_t irq)
{
    assert(irq == 31U || irq == 32U);
    irq_enabled[irq - 31U] = false; ++nvic_writes;
}
void NVIC_ClearPendingIRQ(uint8_t irq)
{
    assert(irq == 31U || irq == 32U); ++nvic_writes;
}
void NVIC_SetPriority(uint8_t irq, uint8_t priority)
{
    assert((irq == 31U || irq == 32U) && priority < 16U);
    irq_priority[irq - 31U] = priority; ++nvic_writes;
}
uint32_t Timebase_Millis(void) { return mock_ms; }

#include "../src/drivers/STM32F334_I2C.c"

static I2C_Handle_t hi2c1;
static unsigned callbacks;
static I2C_Result last_result;
static bool callback_submit;
static I2C_Transaction callback_transaction;
static const uint8_t tx_bytes[] = {0x10U, 0x20U};
static uint8_t rx_bytes[2];

static void record(I2C_Result result, void *context)
{
    assert(mock_mask == 0U && context == &callbacks);
    ++callbacks; last_result = result;
    if (callback_submit) {
        callback_submit = false;
        assert(I2C_Submit(&hi2c1, &callback_transaction));
        assert(!I2C_DeInit(&hi2c1));
        I2C_Process(&hi2c1); /* Recursion must leave the queued request alone. */
    }
}

static I2C_Transaction transaction(uint16_t tx_length, uint16_t rx_length)
{
    I2C_Transaction t = {
        .address = 0x50U, .tx = tx_bytes, .rx = rx_bytes,
        .tx_length = tx_length, .rx_length = rx_length, .timeout_ms = 10U,
        .callback = record, .context = &callbacks
    };
    return t;
}

static void fresh_driver(void)
{
    if (hi2c1.Initialized) {
        assert(!I2C_Busy(&hi2c1));
        assert(I2C_DeInit(&hi2c1));
    }
    memset(&hi2c1, 0, sizeof hi2c1);
    memset(&mock_i2c, 0, sizeof mock_i2c);
    memset(&mock_rcc, 0, sizeof mock_rcc);
    mock_rcc.CR = 1U << 1; /* HSIRDY. */
    mock_ms = callbacks = 0U;
    callback_submit = false;
    hi2c1.pI2Cx = I2C1;
    hi2c1.I2C_Config = (I2C_Config_t){8000000U, I2C_TIMING_100KHZ_HSI8, 5U};
    assert(I2C_Init(&hi2c1));
}

static void event(uint32_t status)
{
    mock_i2c.CR2 &= ~I2C_CR2_START; /* HW consumed START/repeated START. */
    mock_i2c.ISR = status;
    I2C_EVIRQHandling(&hi2c1);
    mock_i2c.ISR = status & I2C_ISR_BUSY; /* Model clear-on-access/W1C. */
}

static void error_event(uint32_t status)
{
    mock_i2c.CR2 &= ~I2C_CR2_START;
    mock_i2c.ISR = status;
    I2C_ERIRQHandling(&hi2c1);
    mock_i2c.ISR = status & I2C_ISR_BUSY;
}

static void complete_tx_byte(void)
{
    event(I2C_ISR_TXIS | I2C_ISR_BUSY);
    event(I2C_ISR_STOPF);
    I2C_Process(&hi2c1);
}

static void test_lifecycle_and_validation(void)
{
    I2C_Handle_t uninitialized = {0};
    I2C_Transaction t = transaction(1U, 0U);
    assert(!I2C_Init(NULL) && !I2C_Init(&uninitialized));
    assert(!I2C_Submit(&uninitialized, &t) && !I2C_DeInit(&uninitialized));
    I2C_Process(&uninitialized); I2C_EVIRQHandling(&uninitialized);
    I2C_ERIRQHandling(&uninitialized);
    assert(nvic_writes == 0U);
    fresh_driver();
    assert(mock_i2c.TIMINGR == 0x10420F13U && mock_i2c.CR1 == I2C_CR1_PE);
    assert(irq_enabled[0] && irq_enabled[1]);
    assert(irq_priority[0] == 5U && irq_priority[1] == 5U);
    I2C_Handle_t other = hi2c1;
    assert(!I2C_Init(&other) && !I2C_Init(&hi2c1));
    I2C_RegDef_t snapshot = mock_i2c;
    unsigned writes = nvic_writes;
    assert(!I2C_Submit(&other, &t) && !I2C_DeInit(&other));
    I2C_Process(&other); I2C_EVIRQHandling(&other); I2C_ERIRQHandling(&other);
    assert(memcmp(&snapshot, &mock_i2c, sizeof snapshot) == 0 && writes == nvic_writes);
    I2C_Transaction invalid = t;
    invalid.tx_length = invalid.rx_length = 0U;
    assert(!I2C_Submit(&hi2c1, &invalid));
    invalid = t; invalid.tx_length = 256U;
    assert(!I2C_Submit(&hi2c1, &invalid));
    invalid = t; invalid.tx = NULL;
    assert(!I2C_Submit(&hi2c1, &invalid));
    invalid = t; invalid.address = 0x80U;
    assert(!I2C_Submit(&hi2c1, &invalid));
    invalid = t; invalid.timeout_ms = 0U;
    assert(!I2C_Submit(&hi2c1, &invalid));
    assert(I2C_DeInit(&hi2c1));
    assert(!irq_enabled[0] && !irq_enabled[1]);
    assert((mock_rcc.APB1ENR & (1U << 21)) == 0U);
    other.I2C_Config.I2C_Timing = 1U << 24;
    assert(!I2C_Init(&other));
    other = hi2c1; other.I2C_Config.I2C_KernelHz = 16000000U;
    assert(!I2C_Init(&other));
    other = hi2c1; mock_rcc.CFGR3 = 1U << 4;
    assert(!I2C_Init(&other));
    mock_rcc.CFGR3 = 0U; mock_rcc.CR = 0U;
    assert(!I2C_Init(&other));
    mock_rcc.CR = 1U << 1;
    assert(I2C_Init(&other));
    assert(I2C_DeInit(&other));
}

static void test_tx_rx_repeated_start(void)
{
    fresh_driver();
    I2C_Transaction t = transaction(2U, 0U);
    assert(I2C_Submit(&hi2c1, &t));
    assert(!I2C_DeInit(&hi2c1));
    I2C_Process(&hi2c1);
    assert((mock_i2c.CR2 & 0xFEU) == 0xA0U); /* Unshifted 0x50 address. */
    assert(((mock_i2c.CR2 >> 16) & 255U) == 2U);
    assert((mock_i2c.CR2 & I2C_CR2_AUTOEND) != 0U);
    event(I2C_ISR_TXIS | I2C_ISR_BUSY);
    assert(mock_i2c.TXDR == 0x10U);
    event(I2C_ISR_TXIS | I2C_ISR_BUSY);
    assert(mock_i2c.TXDR == 0x20U && callbacks == 0U);
    assert((mock_i2c.CR1 & I2C_CR1_TXIE) == 0U);
    event(I2C_ISR_STOPF);
    assert(callbacks == 0U); /* STOP ISR does not invoke app. */
    I2C_Process(&hi2c1);
    assert(callbacks == 1U && last_result == I2C_OK);

    t = transaction(0U, 1U);
    assert(I2C_Submit(&hi2c1, &t)); I2C_Process(&hi2c1);
    assert((mock_i2c.CR2 & (I2C_CR2_READ | I2C_CR2_AUTOEND)) ==
           (I2C_CR2_READ | I2C_CR2_AUTOEND));
    mock_i2c.RXDR = 0xABU;
    event(I2C_ISR_RXNE | I2C_ISR_STOPF); /* Last RX and STOP can be co-pending. */
    I2C_Process(&hi2c1);
    assert(callbacks == 2U && last_result == I2C_OK && rx_bytes[0] == 0xABU);

    t = transaction(1U, 2U);
    assert(I2C_Submit(&hi2c1, &t)); I2C_Process(&hi2c1);
    assert((mock_i2c.CR2 & I2C_CR2_AUTOEND) == 0U);
    event(I2C_ISR_TXIS | I2C_ISR_BUSY);
    event(I2C_ISR_TC | I2C_ISR_BUSY);
    assert((mock_i2c.CR2 & I2C_CR2_START) != 0U);
    assert((mock_i2c.CR2 & I2C_CR2_STOP) == 0U);
    assert((mock_i2c.CR2 & I2C_CR2_READ) != 0U && hi2c1.State == I2C_STATE_RX);
    mock_i2c.RXDR = 0x12U; event(I2C_ISR_RXNE | I2C_ISR_BUSY);
    mock_i2c.RXDR = 0x34U; event(I2C_ISR_RXNE | I2C_ISR_STOPF);
    I2C_Process(&hi2c1);
    assert(callbacks == 3U && last_result == I2C_OK);
    assert(rx_bytes[0] == 0x12U && rx_bytes[1] == 0x34U);
    I2C_Stats stats; I2C_GetStats(&hi2c1, &stats);
    assert(stats.completed == 3U && stats.submitted == 3U);
}

static void test_nack_errors_arbitration(void)
{
    fresh_driver();
    I2C_Transaction t = transaction(1U, 1U);
    assert(I2C_Submit(&hi2c1, &t)); I2C_Process(&hi2c1);
    event(I2C_ISR_NACKF | I2C_ISR_BUSY);
    assert(hi2c1.State == I2C_STATE_WAIT_STOP && callbacks == 0U);
    assert((mock_i2c.CR2 & I2C_CR2_STOP) == 0U); /* NACK generates own STOP. */
    I2C_Process(&hi2c1); assert(callbacks == 0U);
    event(I2C_ISR_STOPF); I2C_Process(&hi2c1);
    assert(callbacks == 1U && last_result == I2C_NACK);

    t = transaction(1U, 0U);
    assert(I2C_Submit(&hi2c1, &t)); I2C_Process(&hi2c1);
    error_event(I2C_ISR_BERR | I2C_ISR_BUSY);
    assert((mock_i2c.CR2 & I2C_CR2_STOP) != 0U && callbacks == 1U);
    event(I2C_ISR_STOPF); I2C_Process(&hi2c1);
    assert(callbacks == 2U && last_result == I2C_BUS_ERROR);
    assert(I2C_Submit(&hi2c1, &t)); I2C_Process(&hi2c1);
    error_event(I2C_ISR_OVR | I2C_ISR_BUSY);
    event(I2C_ISR_STOPF); I2C_Process(&hi2c1);
    assert(callbacks == 3U && last_result == I2C_OVERRUN);

    assert(I2C_Submit(&hi2c1, &t)); I2C_Process(&hi2c1);
    assert(I2C_Submit(&hi2c1, &t)); /* Next request must wait for winning master. */
    error_event(I2C_ISR_ARLO | I2C_ISR_BUSY);
    assert((mock_i2c.CR2 & I2C_CR2_STOP) == 0U);
    I2C_Process(&hi2c1);
    assert(callbacks == 4U && last_result == I2C_ARBITRATION_LOST);
    assert(hi2c1.State == I2C_STATE_WAIT_BUS && !hi2c1.BusOwned);
    assert((mock_i2c.CR2 & I2C_CR2_START) == 0U);
    mock_i2c.ISR = 0U; I2C_Process(&hi2c1);
    complete_tx_byte();
    assert(callbacks == 5U && last_result == I2C_OK);

    assert(I2C_Submit(&hi2c1, &t)); I2C_Process(&hi2c1);
    event(I2C_ISR_STOPF); I2C_Process(&hi2c1); /* Premature STOP is not success. */
    assert(callbacks == 6U && last_result == I2C_BUS_ERROR);

    t = transaction(1U, 1U);
    assert(I2C_Submit(&hi2c1, &t)); I2C_Process(&hi2c1);
    event(I2C_ISR_TXIS | I2C_ISR_BUSY);
    event(I2C_ISR_TC | I2C_ISR_STOPF);
    assert((mock_i2c.CR2 & I2C_CR2_START) == 0U); /* STOP forbids repeated START. */
    I2C_Process(&hi2c1);
    assert(callbacks == 7U && last_result == I2C_BUS_ERROR);

    t = transaction(1U, 0U);
    assert(I2C_Submit(&hi2c1, &t)); I2C_Process(&hi2c1);
    event(I2C_ISR_TXIS | I2C_ISR_STOPF);
    assert(hi2c1.TxCount == 0U); /* Do not feed another byte after STOP. */
    I2C_Process(&hi2c1);
    assert(callbacks == 8U && last_result == I2C_BUS_ERROR);
}

static void test_timeouts_and_busy_bus(void)
{
    fresh_driver();
    I2C_Transaction t = transaction(1U, 0U);
    mock_ms = 0xFFFFFFF0U;
    mock_i2c.ISR = I2C_ISR_BUSY;
    assert(I2C_Submit(&hi2c1, &t)); I2C_Process(&hi2c1);
    assert(hi2c1.State == I2C_STATE_WAIT_BUS && mock_i2c.CR2 == 0U);
    mock_ms = 0x10U; I2C_Process(&hi2c1);
    assert(callbacks == 1U && last_result == I2C_TIMEOUT);
    assert(mock_i2c.CR2 == 0U && hi2c1.Stats.recoveries == 0U);

    mock_i2c.ISR = 0U;
    assert(I2C_Submit(&hi2c1, &t)); I2C_Process(&hi2c1);
    mock_i2c.ISR = I2C_ISR_BUSY;
    mock_ms += t.timeout_ms; I2C_Process(&hi2c1);
    assert(callbacks == 1U && hi2c1.State == I2C_STATE_WAIT_STOP);
    assert((mock_i2c.CR2 & I2C_CR2_STOP) != 0U);
    event(I2C_ISR_STOPF); I2C_Process(&hi2c1);
    assert(callbacks == 2U && last_result == I2C_TIMEOUT);

    assert(I2C_Submit(&hi2c1, &t)); I2C_Process(&hi2c1);
    assert(I2C_Submit(&hi2c1, &t));
    mock_i2c.ISR = I2C_ISR_BUSY;
    mock_ms += t.timeout_ms; I2C_Process(&hi2c1);
    ++mock_ms; error_event(I2C_ISR_BERR | I2C_ISR_BUSY);
    assert(hi2c1.Result == I2C_TIMEOUT); /* Later errors cannot extend abort. */
    mock_ms += I2C_ABORT_GRACE_MS - 1U; I2C_Process(&hi2c1); /* STOP never arrived. */
    assert(callbacks == 3U && last_result == I2C_TIMEOUT);
    assert(hi2c1.Stats.recoveries == 1U && hi2c1.State == I2C_STATE_WAIT_BUS);
    assert(mock_i2c.CR2 == 0U); /* No restart while BUSY remains asserted. */
    mock_i2c.ISR = 0U; I2C_Process(&hi2c1);
    complete_tx_byte();
    assert(callbacks == 4U && last_result == I2C_OK);
}

static void test_queue_and_callback(void)
{
    fresh_driver();
    I2C_Transaction t = transaction(1U, 0U);
    for (unsigned i = 0U; i < I2C_TRANSACTION_QUEUE_SIZE; ++i)
        assert(I2C_Submit(&hi2c1, &t));
    assert(!I2C_Submit(&hi2c1, &t));
    I2C_Process(&hi2c1);
    assert(I2C_Submit(&hi2c1, &t)); /* One active + four pending. */
    for (unsigned i = 0U; i < 5U; ++i) complete_tx_byte();
    assert(callbacks == 5U && !I2C_Busy(&hi2c1));
    callback_submit = true; callback_transaction = t;
    assert(I2C_Submit(&hi2c1, &t)); I2C_Process(&hi2c1);
    complete_tx_byte();
    assert(callbacks == 6U && hi2c1.QueueCount == 1U);
    I2C_Process(&hi2c1); complete_tx_byte();
    assert(callbacks == 7U && !I2C_Busy(&hi2c1));
    assert(I2C_DeInit(&hi2c1));
}

int main(void)
{
    test_lifecycle_and_validation();
    test_tx_rx_repeated_start();
    test_nack_errors_arbitration();
    test_timeouts_and_busy_bus();
    test_queue_and_callback();
    assert(mock_mask == 0U);
    puts("I2C handle/IRQ/queue/protocol/timeout mock tests passed");
    return 0;
}
