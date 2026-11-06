/* Host test: simulate register events, not the electrical SPI bus.
 * Build with a native C11 compiler; no MCU toolchain needed for this file.
 */
#include "../src/drivers/STM32F334_SPI.h"
#include "../src/drivers/STM32F334_CORE.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

static SPI_RegDef_t mock_spi;
static RCC_RegDef_t mock_rcc;
static GPIO_RegDef_t mock_gpioa, mock_gpiob, mock_gpioc, mock_gpiod, mock_gpiof;
static uint32_t mock_ms;
static uint32_t mock_irq_mask;
static uint8_t mock_irq_priority;
static unsigned mock_nvic_operations;
static bool mock_irq_enabled;

/* Replace only register addresses. Compile the actual driver/state machine. */
#undef SPI1
#undef RCC
#undef GPIOA
#undef GPIOB
#undef GPIOC
#undef GPIOD
#undef GPIOF
#define SPI1 (&mock_spi)
#define RCC (&mock_rcc)
#define GPIOA (&mock_gpioa)
#define GPIOB (&mock_gpiob)
#define GPIOC (&mock_gpioc)
#define GPIOD (&mock_gpiod)
#define GPIOF (&mock_gpiof)

uint32_t IRQ_Save(void)
{
    uint32_t saved = mock_irq_mask;
    mock_irq_mask = 1U;
    return saved;
}

void IRQ_Restore(uint32_t saved)
{
    mock_irq_mask = saved;
}

void NVIC_EnableIRQ(uint8_t irq)
{
    assert(irq == 35U);
    mock_irq_enabled = true;
    ++mock_nvic_operations;
}
void NVIC_DisableIRQ(uint8_t irq)
{
    assert(irq == 35U);
    mock_irq_enabled = false;
    ++mock_nvic_operations;
}
void NVIC_ClearPendingIRQ(uint8_t irq)
{
    assert(irq == 35U);
    ++mock_nvic_operations;
}
void NVIC_SetPriority(uint8_t irq, uint8_t priority)
{
    assert(irq == 35U && priority < 16U);
    mock_irq_priority = priority;
    ++mock_nvic_operations;
}

uint32_t Timebase_Millis(void) { return mock_ms; }

#include "../src/drivers/STM32F334_SPI.c"

static SPI_Handle_t hspi1;
static unsigned callbacks;
static SPI_Result last_result;
static bool enqueue_in_callback;
static SPI_Transaction reentrant_transaction;

static void record_callback(SPI_Result result, void *context)
{
    assert(mock_irq_mask == 0U); /* User code runs after interrupts restored. */
    assert(context == &callbacks);
    ++callbacks;
    last_result = result;
    if (enqueue_in_callback) {
        enqueue_in_callback = false;
        assert(SPI_Submit(&hspi1, &reentrant_transaction));
        SPI_Process(&hspi1); /* Recursion is intentionally ignored. */
        assert(!SPI_Init(&hspi1)); /* Reinit must not disturb callback/queue. */
        assert(!SPI_DeInit(&hspi1)); /* Pending callback/Process still owns handle. */
    }
}

static SPI_Transaction transaction(const uint8_t *tx, uint8_t *rx, uint16_t length)
{
    SPI_Transaction t = {
        .cs_port = GPIOB, .cs_pin = 6U,
        .tx = tx, .rx = rx, .length = length, .timeout_ms = 100U,
        .callback = record_callback, .context = &callbacks
    };
    return t;
}

static uint8_t transmit_irq(void)
{
    mock_spi.SR = SPI_SR_TXE;
    SPI_IRQHandling(&hspi1);
    return (uint8_t)mock_spi.DR;
}

static void receive_irq(uint8_t value, bool busy)
{
    mock_spi.DR = value;
    mock_spi.SR = SPI_SR_TXE | SPI_SR_RXNE | (busy ? SPI_SR_BSY : 0U);
    SPI_IRQHandling(&hspi1);
}

static void finish_bus(void)
{
    mock_spi.SR = SPI_SR_TXE;
    SPI_Process(&hspi1);
}

static void complete_one_byte(void)
{
    (void)transmit_irq();
    receive_irq(0xA5U, false);
    finish_bus();
}

int main(void)
{
    const uint8_t tx[] = {0x11U, 0x22U};
    uint8_t rx[2] = {0U, 0U};
    SPI_Transaction t = transaction(tx, rx, 2U);
    hspi1.pSPIx = SPI1;
    hspi1.SPI_Config = (SPI_Config_t){
        .SPI_DeviceMode = SPI_DEVICE_MODE_MASTER,
        .SPI_DataFrameSize = SPI_DFF_8BITS,
        .SPI_CPOL = SPI_CPOL_LOW,
        .SPI_CPHA = SPI_CPHA_FIRST_EDGE,
        .SPI_SclkSpeed = SPI_SCLK_SPEED_DIV16,
        .SPI_IRQPriority = 3U
    };
    SPI_Handle_t other = hspi1;
    SPI_Handle_t uninitialized = {0};
    SPI_RegDef_t unsupported_regs = {0};
    other.pSPIx = &unsupported_regs;
    assert(!SPI_Init(NULL) && !SPI_Init(&uninitialized) && !SPI_Init(&other));
    assert(!SPI_Submit(&uninitialized, &t));
    SPI_Process(&uninitialized);
    SPI_IRQHandling(&uninitialized);
    assert(!SPI_Busy(&uninitialized) && !SPI_DeInit(&uninitialized));
    assert(mock_nvic_operations == 0U && mock_rcc.APB2ENR == 0U);
    other = hspi1;
    other.SPI_Config.SPI_IRQPriority = 16U;
    assert(!SPI_Init(&other));
    other.SPI_Config.SPI_IRQPriority = 3U;
    other.SPI_Config.SPI_DeviceMode = 0U;
    assert(!SPI_Init(&other));
    other = hspi1;
    other.SPI_Config.SPI_DataFrameSize = 1U;
    assert(!SPI_Init(&other));
    other = hspi1;
    other.SPI_Config.SPI_CPOL = 2U;
    assert(!SPI_Init(&other));
    other = hspi1;
    other.SPI_Config.SPI_CPHA = 2U;
    assert(!SPI_Init(&other));
    other = hspi1;
    other.SPI_Config.SPI_SclkSpeed = 8U;
    assert(!SPI_Init(&other));
    other = hspi1;
    mock_rcc.APB2ENR = 1U << 18; /* Clock helper preserves unrelated RCC bits. */
    SPI_PeriClockControl(SPI1, ENABLE);
    assert(mock_rcc.APB2ENR == ((1U << 18) | (1U << 12)));
    SPI_PeriClockControl(SPI1, DISABLE);
    assert(mock_rcc.APB2ENR == (1U << 18));
    SPI_PeriClockControl(NULL, ENABLE);
    SPI_PeriClockControl(&unsupported_regs, ENABLE);
    SPI_PeriClockControl(SPI1, 2U);
    assert(mock_rcc.APB2ENR == (1U << 18));
    mock_spi.SR = SPI_SR_TXE;
    assert(SPI_Init(&hspi1));
    assert(mock_irq_priority == 3U && mock_irq_enabled);
    assert((mock_rcc.APB2ENR & (1U << 12)) != 0U);
    assert((mock_spi.CR1 & 3U) == 0U && ((mock_spi.CR1 >> 3) & 7U) == 3U);
    assert(!SPI_Init(&hspi1) && !SPI_Init(&other));
    SPI_RegDef_t snapshot = mock_spi;
    unsigned operations = mock_nvic_operations;
    assert(!SPI_Submit(&other, &t) && !SPI_DeInit(&other));
    SPI_Process(&other);
    SPI_IRQHandling(&other);
    assert(!SPI_Busy(&other));
    assert(memcmp(&snapshot, &mock_spi, sizeof snapshot) == 0);
    assert(mock_nvic_operations == operations);
    assert(!SPI_Busy(&hspi1));
    assert(!SPI_Submit(&hspi1, NULL));
    SPI_Transaction invalid = t;
    invalid.cs_pin = 16U;
    assert(!SPI_Submit(&hspi1, &invalid));
    invalid = t;
    invalid.length = 0U;
    assert(!SPI_Submit(&hspi1, &invalid));
    invalid = t;
    invalid.timeout_ms = 0U;
    assert(!SPI_Submit(&hspi1, &invalid));

    /* One RX response unlocks exactly one further TX; final RX is not yet
     * wire completion if BSY is still set. */
    assert(SPI_Submit(&hspi1, &t));
    assert(!SPI_DeInit(&hspi1)); /* Queued buffers cannot be abandoned. */
    SPI_Process(&hspi1);
    assert(SPI_Busy(&hspi1) && mock_gpiob.BSRR == (1U << 22));
    assert(transmit_irq() == 0x11U);
    assert((mock_spi.CR2 & SPI_CR2_TXEIE) == 0U);
    assert(transmit_irq() == 0x11U); /* No premature second TX / IRQ storm. */
    receive_irq(0xA1U, false);
    assert((uint8_t)mock_spi.DR == 0x22U);
    receive_irq(0xA2U, true);
    SPI_Process(&hspi1);
    assert(callbacks == 0U && SPI_Busy(&hspi1));
    finish_bus();
    assert(callbacks == 1U && last_result == SPI_OK && !SPI_Busy(&hspi1));
    assert(rx[0] == 0xA1U && rx[1] == 0xA2U);
    assert(mock_gpiob.BSRR == (1U << 6));

    /* RX-only sends dummy bytes; TX-only still drains RX. */
    t = transaction(NULL, rx, 1U);
    assert(SPI_DeInit(&hspi1));
    hspi1.SPI_Config.SPI_CPOL = SPI_CPOL_HIGH;
    hspi1.SPI_Config.SPI_CPHA = SPI_CPHA_SECOND_EDGE;
    hspi1.SPI_Config.SPI_SclkSpeed = SPI_SCLK_SPEED_DIV256;
    hspi1.SPI_Config.SPI_IRQPriority = 5U;
    assert(SPI_Init(&hspi1));
    assert(mock_irq_priority == 5U);
    assert(SPI_Submit(&hspi1, &t));
    SPI_Process(&hspi1);
    assert((mock_spi.CR1 & 3U) == 3U);
    assert(((mock_spi.CR1 >> 3) & 7U) == 7U);
    assert(transmit_irq() == 0xFFU);
    receive_irq(0x42U, false);
    finish_bus();
    assert(rx[0] == 0x42U && callbacks == 2U);

    /* Queue capacity and wraparound: 4 waiting + 1 active, ordered callbacks. */
    t = transaction(tx, NULL, 1U);
    for (unsigned i = 0U; i < 4U; ++i) {
        assert(SPI_Submit(&hspi1, &t));
    }
    assert(!SPI_Submit(&hspi1, &t));
    SPI_Process(&hspi1);
    assert(SPI_Submit(&hspi1, &t));
    assert(!SPI_Submit(&hspi1, &t));
    assert(!SPI_Init(&hspi1));
    for (unsigned i = 0U; i < 5U; ++i) {
        complete_one_byte();
    }
    assert(callbacks == 7U && !SPI_Busy(&hspi1));

    /* Callback can queue another transfer without interrupt lock or recursion. */
    reentrant_transaction = t;
    enqueue_in_callback = true;
    assert(SPI_Submit(&hspi1, &t));
    SPI_Process(&hspi1);
    complete_one_byte();
    assert(callbacks == 8U && SPI_Busy(&hspi1));
    SPI_Process(&hspi1);
    complete_one_byte();
    assert(callbacks == 9U && !SPI_Busy(&hspi1));

    /* Unsigned timeout survives uint32 tick rollover and driver can recover. */
    mock_ms = 0xFFFFFFF0U;
    t.timeout_ms = 25U;
    assert(SPI_Submit(&hspi1, &t));
    SPI_Process(&hspi1);
    mock_ms = 0x20U;
    SPI_Process(&hspi1);
    assert(callbacks == 10U && last_result == SPI_TIMEOUT && !SPI_Busy(&hspi1));
    assert(mock_gpiob.BSRR == (1U << 6));
    assert(SPI_Submit(&hspi1, &t));
    SPI_Process(&hspi1);
    complete_one_byte();
    assert(callbacks == 11U && last_result == SPI_OK);

    /* Error callback is deferred, and the next transaction succeeds. */
    assert(SPI_Submit(&hspi1, &t));
    SPI_Process(&hspi1);
    mock_spi.SR = SPI_SR_OVR;
    SPI_IRQHandling(&hspi1);
    assert(callbacks == 11U);
    SPI_Process(&hspi1);
    assert(callbacks == 12U && last_result == SPI_HW_ERROR && !SPI_Busy(&hspi1));
    assert(SPI_Submit(&hspi1, &t));
    mock_spi.SR = SPI_SR_TXE;
    SPI_Process(&hspi1);
    complete_one_byte();
    assert(callbacks == 13U && last_result == SPI_OK);

    /* A stuck BSY after final RX has a deadline, rather than blocking ISR. */
    assert(SPI_Submit(&hspi1, &t));
    SPI_Process(&hspi1);
    (void)transmit_irq();
    receive_irq(0xA5U, true);
    SPI_Process(&hspi1);
    assert(callbacks == 13U);
    mock_ms += t.timeout_ms;
    SPI_Process(&hspi1);
    assert(callbacks == 14U && last_result == SPI_TIMEOUT && !SPI_Busy(&hspi1));
    assert(SPI_DeInit(&hspi1));
    assert(!hspi1.Initialized && !mock_irq_enabled);
    assert((mock_rcc.APB2ENR & (1U << 12)) == 0U);
    snapshot = mock_spi;
    operations = mock_nvic_operations;
    assert(!SPI_Submit(&hspi1, &t) && !SPI_DeInit(&hspi1));
    SPI_Process(&hspi1);
    SPI_IRQHandling(&hspi1);
    assert(memcmp(&snapshot, &mock_spi, sizeof snapshot) == 0);
    assert(mock_nvic_operations == operations);

    /* Ownership is released by DeInit, and the new handle gets fresh state. */
    other.QueueCount = 3U;
    assert(SPI_Init(&other));
    assert(other.Initialized && other.QueueCount == 0U);
    assert(mock_irq_priority == 3U);
    assert(!SPI_Init(&hspi1) && !SPI_DeInit(&hspi1));
    assert(SPI_DeInit(&other));
    assert(SPI_Init(&hspi1) && SPI_DeInit(&hspi1));
    assert(mock_irq_mask == 0U);
    puts("SPI handle/config/ownership/register-mock tests passed");
    return 0;
}
