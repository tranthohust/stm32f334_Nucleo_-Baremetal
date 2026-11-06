/* Native register-mock tests: run the actual UART driver, no electrical bus.
 * USART/GPIO/RCC are objects; private DMA registers get one host virtual page
 * at their documented MCU address. No real peripheral access occurs on a PC.
 * Helpers explicitly emulate CNDTR/reload, DMA W1C and USART read/ICR effects.
 */
#ifndef _WIN32
#define _DEFAULT_SOURCE
#endif
#include "../src/drivers/STM32F334_UART.h"
#include "../src/drivers/STM32F334_CORE.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#else
#include <sys/mman.h>
#endif

static USART_RegDef_t mock_uart;
static RCC_RegDef_t mock_rcc;
static uint32_t mock_irq_mask;
static bool mock_nvic_enabled[96];
static uint8_t mock_nvic_priority[96];
static UART_Handle_t test_uart;
static UART_Handle_t *test_handle = &test_uart;

#undef USART2
#undef RCC
#define USART2 (&mock_uart)
#define RCC (&mock_rcc)

uint32_t IRQ_Save(void)
{
    uint32_t saved = mock_irq_mask;
    mock_irq_mask = 1U;
    return saved;
}

void IRQ_Restore(uint32_t saved) { mock_irq_mask = saved; }

static void check_irq(uint8_t irq)
{
    assert(irq == 16U || irq == 17U || irq == 38U);
}

void NVIC_EnableIRQ(uint8_t irq)
{
    check_irq(irq);
    mock_nvic_enabled[irq] = true;
}

void NVIC_DisableIRQ(uint8_t irq)
{
    check_irq(irq);
    mock_nvic_enabled[irq] = false;
}

void NVIC_ClearPendingIRQ(uint8_t irq) { check_irq(irq); }
void NVIC_SetPriority(uint8_t irq, uint8_t priority)
{
    check_irq(irq);
    mock_nvic_priority[irq] = priority;
}

#include "../src/drivers/STM32F334_UART.c"

static void *map_dma_registers(void)
{
    void *requested = (void *)(uintptr_t)0x40020000U;
#ifdef _WIN32
    void *mapped = VirtualAlloc(requested, 4096U, MEM_RESERVE | MEM_COMMIT,
                               PAGE_READWRITE);
#else
    /* A hint without MAP_FIXED cannot replace an existing host allocation. */
    void *mapped = mmap(requested, 4096U, PROT_READ | PROT_WRITE,
                        MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
#endif
    assert(mapped == requested);
    return mapped;
}

static void unmap_dma_registers(void *mapped)
{
#ifdef _WIN32
    assert(VirtualFree(mapped, 0U, MEM_RELEASE) != 0);
#else
    assert(munmap(mapped, 4096U) == 0);
#endif
}

/* DMA GIF is the OR of the channel's TC/HT/TE; clearing CGIF clears all four. */
static void apply_dma_w1c(void)
{
    uint32_t clear = UART_DMA_IFCR;
    for (unsigned shift = 20U; shift <= 24U; shift += 4U) {
        if ((clear & (1U << shift)) != 0U)
            UART_DMA_ISR &= ~(15U << shift);
        else
            UART_DMA_ISR &= ~(clear & (14U << shift));
        UART_DMA_ISR &= ~(1U << shift);
        if ((UART_DMA_ISR & (14U << shift)) != 0U)
            UART_DMA_ISR |= 1U << shift;
    }
    UART_DMA_IFCR = 0U;
}

static void apply_uart_effects(void)
{
    mock_uart.ISR &= ~mock_uart.ICR;
    mock_uart.ICR = 0U;
    if ((mock_uart.RQR & UART_RXFRQ) != 0U)
        mock_uart.ISR &= ~UART_RXNE;
    mock_uart.RQR = 0U;
    apply_dma_w1c();
    assert(mock_irq_mask == 0U);
}

static void init_uart(UART_RxMode mode)
{
    if (test_handle->Initialized) {
        assert(UART_DeInit(test_handle));
        apply_uart_effects();
    }
    memset(test_handle, 0, sizeof *test_handle);
    test_handle->pUSARTx = USART2;
    test_handle->UART_Config = (UART_Config_t){
        .UART_PCLKHz = 8000000U,
        .UART_BaudRate = 115200U,
        .UART_RxMode = mode,
        .UART_IRQPriority = 5U
    };
    memset(&mock_uart, 0, sizeof mock_uart);
    mock_uart.ISR = UART_ACKS;
    assert(UART_Init(test_handle));
    apply_uart_effects();
    assert(mock_uart.BRR == 69U);
    assert(mock_nvic_enabled[38] && mock_nvic_enabled[17]);
    assert(mock_nvic_enabled[16] == (mode == UART_RX_DMA));
    assert(mock_nvic_priority[16] == 5U && mock_nvic_priority[17] == 5U &&
           mock_nvic_priority[38] == 5U);
    assert(((mock_uart.CR1 & UART_RXNEIE) != 0U) == (mode == UART_RX_IRQ));
    assert(((mock_uart.CR3 & UART_DMAR) != 0U) == (mode == UART_RX_DMA));
}

static void irq_receive(uint8_t value)
{
    assert((mock_uart.CR1 & UART_RXNEIE) != 0U);
    mock_uart.RDR = value;
    mock_uart.ISR |= UART_RXNE;
    UART_IRQHandling(test_handle);
    /* RAM cannot emulate read-to-clear RDR, so the hardware model does it. */
    mock_uart.ISR &= ~UART_RXNE;
    apply_uart_effects();
}

static uint8_t rx_pattern(unsigned sequence)
{
    return (uint8_t)((sequence * 37U + 11U) & 255U);
}

static void dma_receive(unsigned start, unsigned count, bool service_irq)
{
    assert((mock_uart.CR3 & UART_DMAR) != 0U);
    assert((UART_DMA_RX->CCR & DMA_EN) != 0U);
    for (unsigned i = 0U; i < count; ++i) {
        /* Translate the 32-bit MCU address into the host's real array. */
        assert(UART_DMA_RX->CMAR == (uint32_t)(uintptr_t)test_handle->RxDMABuffer);
        uint32_t offset = RAW_SIZE - UART_DMA_RX->CNDTR;
        assert(offset < RAW_SIZE);
        test_handle->RxDMABuffer[offset] = rx_pattern(start + i);
        --UART_DMA_RX->CNDTR;
        if (UART_DMA_RX->CNDTR == RAW_SIZE / 2U)
            UART_DMA_ISR |= RX_DMA_HT | (1U << 20);
        if (UART_DMA_RX->CNDTR == 0U) {
            UART_DMA_ISR |= RX_DMA_TC | (1U << 20);
            UART_DMA_RX->CNDTR = RAW_SIZE; /* Circular auto reload. */
        }
        if (service_irq && (UART_DMA_ISR & (RX_DMA_HT | RX_DMA_TC)) != 0U) {
            UART_DMARxIRQHandling(test_handle);
            apply_uart_effects();
        }
    }
}

static void signal_idle(void)
{
    mock_uart.ISR |= UART_IDLE;
    UART_IRQHandling(test_handle);
    apply_uart_effects();
}

static void expect_rx(unsigned start, size_t count)
{
    uint8_t output[600];
    assert(count <= sizeof output);
    assert(UART_Read(test_handle, output, sizeof output) == count);
    for (size_t i = 0U; i < count; ++i)
        assert(output[i] == rx_pattern(start + (unsigned)i));
    assert(UART_Read(test_handle, output, sizeof output) == 0U);
}

static void run_irq_rx_tests(void)
{
    init_uart(UART_RX_IRQ);
    uint8_t first[137];
    for (unsigned i = 0U; i < 200U; ++i)
        irq_receive(rx_pattern(i));
    assert(UART_Read(test_handle, first, sizeof first) == sizeof first);
    for (unsigned i = 0U; i < sizeof first; ++i)
        assert(first[i] == rx_pattern(i));
    for (unsigned i = 200U; i < 600U; ++i)
        irq_receive(rx_pattern(i));
    expect_rx(137U, 463U); /* Physical ring wraps, stream order is preserved. */

    for (unsigned i = 0U; i < 600U; ++i)
        irq_receive(rx_pattern(i));
    UART_Stats snapshot;
    UART_GetStats(test_handle, &snapshot);
    assert(snapshot.rx_bytes == 1200U && snapshot.rx_dropped == 89U);
    assert(snapshot.rx_epoch != 0U);
    expect_rx(0U, 511U); /* Drop newest must preserve the older unread bytes. */

    irq_receive('A');
    UART_DiscardRx(test_handle);
    expect_rx(0U, 0U);
    uint32_t epoch = UART_RxEpoch(test_handle);
    irq_receive('B');
    mock_uart.ISR |= 1U << 3; /* ORE: a missing byte breaks framing. */
    UART_IRQHandling(test_handle);
    apply_uart_effects();
    assert(UART_RxEpoch(test_handle) == epoch + 1U);
    UART_GetStats(test_handle, &snapshot);
    assert(snapshot.rx_errors == 1U);
    expect_rx(0U, 0U); /* Old and new fragments cannot be joined across ORE. */
    irq_receive(rx_pattern(9U));
    expect_rx(9U, 1U);
}

static void run_dma_rx_tests(void)
{
    init_uart(UART_RX_DMA);
    dma_receive(0U, 20U, true);
    expect_rx(0U, 0U); /* Short data waits for IDLE or the next HT. */
    signal_idle();
    expect_rx(0U, 20U);
    signal_idle();
    expect_rx(0U, 0U); /* Duplicate events cannot duplicate delivered bytes. */
    dma_receive(20U, 108U, true);
    expect_rx(20U, 108U);
    dma_receive(128U, 128U, true);
    expect_rx(128U, 128U);
    signal_idle();
    expect_rx(0U, 0U);

    dma_receive(256U, 300U, true);
    signal_idle();
    expect_rx(256U, 300U);

    /* IDLE and DMA IRQ refer to the same progress; IDLE can run first. */
    init_uart(UART_RX_DMA);
    dma_receive(0U, 256U, false);
    signal_idle();
    UART_DMARxIRQHandling(test_handle);
    apply_uart_effects();
    expect_rx(0U, 256U); /* Equal modulo positions still mean one full lap. */

    /* TC momentarily visible with CNDTR=0, before circular reload. */
    init_uart(UART_RX_DMA);
    dma_receive(0U, 256U, false);
    UART_DMA_RX->CNDTR = 0U;
    signal_idle();
    expect_rx(0U, 256U);
    signal_idle();
    expect_rx(0U, 0U); /* Already-consumed TC cannot create another lap at zero. */
    UART_DMA_RX->CNDTR = RAW_SIZE;
    signal_idle();
    expect_rx(0U, 0U);

    /* Single delayed wrap is detectable: overwritten input is not delivered. */
    init_uart(UART_RX_DMA);
    dma_receive(0U, 300U, false);
    UART_DMARxIRQHandling(test_handle);
    apply_uart_effects();
    expect_rx(0U, 0U);
    UART_Stats snapshot;
    UART_GetStats(test_handle, &snapshot);
    assert(snapshot.rx_dma_overruns == 1U && snapshot.rx_dropped == 300U);
    assert(snapshot.rx_epoch == 1U);

    /* DMA transfer error restarts the channel and reports framing loss. */
    init_uart(UART_RX_DMA);
    dma_receive(0U, 12U, false);
    UART_DMA_ISR |= RX_DMA_TE | (1U << 20);
    UART_DMARxIRQHandling(test_handle);
    apply_uart_effects();
    UART_GetStats(test_handle, &snapshot);
    assert(snapshot.dma_errors == 1U && snapshot.rx_epoch == 1U);
    assert(UART_DMA_RX->CNDTR == RAW_SIZE && (UART_DMA_RX->CCR & DMA_EN) != 0U);
    dma_receive(40U, 5U, false);
    signal_idle();
    expect_rx(40U, 5U);

    dma_receive(50U, 3U, false);
    signal_idle();
    mock_uart.ISR |= 1U << 1; /* FE: discard potentially corrupt RX data. */
    UART_IRQHandling(test_handle);
    apply_uart_effects();
    assert(UART_RxEpoch(test_handle) == 2U);
    expect_rx(0U, 0U);
    dma_receive(70U, 2U, false);
    signal_idle();
    expect_rx(70U, 2U);
}

static const uint8_t *dma_tx_data(void)
{
    uint32_t offset = UART_DMA_TX->CMAR - (uint32_t)(uintptr_t)test_handle->TxBuffer;
    assert(offset < sizeof test_handle->TxBuffer);
    assert(UART_DMA_TX->CPAR == (uint32_t)(uintptr_t)&mock_uart.TDR);
    assert((UART_DMA_TX->CCR & DMA_EN) != 0U);
    assert(offset + UART_DMA_TX->CNDTR <= sizeof test_handle->TxBuffer);
    return &test_handle->TxBuffer[offset];
}

static void finish_tx(bool error, uint32_t remaining)
{
    UART_DMA_TX->CNDTR = remaining;
    UART_DMA_ISR |= (error ? TX_DMA_TE : TX_DMA_TC) | (1U << 24);
    UART_DMATxIRQHandling(test_handle);
    apply_uart_effects();
}

static void run_tx_tests(void)
{
    init_uart(UART_RX_IRQ);
    uint8_t input[400];
    for (unsigned i = 0U; i < sizeof input; ++i)
        input[i] = rx_pattern(i);
    assert(UART_Write(test_handle, input, sizeof input) == 255U);
    apply_uart_effects();
    assert(UART_DMA_TX->CNDTR == 255U);
    const uint8_t *in_flight = dma_tx_data();
    for (unsigned i = 0U; i < 255U; ++i)
        assert(in_flight[i] == rx_pattern(i));
    memset(input, 0xFF, sizeof input);
    for (unsigned i = 0U; i < 255U; ++i)
        assert(in_flight[i] == rx_pattern(i)); /* Write owns a copy. */

    assert(UART_Write(test_handle, input, 1U) == 0U);
    apply_uart_effects();
    UART_DMA_TX->CNDTR = 155U; /* Partial progress does not release RAM. */
    assert(UART_Write(test_handle, input, 1U) == 0U);
    apply_uart_effects();
    assert(dma_tx_data() == in_flight);
    UART_Stats snapshot;
    UART_GetStats(test_handle, &snapshot);
    assert(snapshot.tx_dropped == 147U && snapshot.tx_bytes == 0U);

    finish_tx(false, 0U);
    assert(!UART_TxIdle(test_handle)); /* DMA complete is earlier than final stop bit. */
    mock_uart.ISR |= UART_TC;
    assert(UART_TxIdle(test_handle));
    UART_GetStats(test_handle, &snapshot);
    assert(snapshot.tx_bytes == 255U);

    const uint8_t wrapped[] = {0x11U, 0x22U, 0x33U, 0x44U};
    assert(UART_Write(test_handle, wrapped, sizeof wrapped) == sizeof wrapped);
    apply_uart_effects();
    assert(UART_DMA_TX->CNDTR == 1U && dma_tx_data()[0] == wrapped[0]);
    finish_tx(false, 0U);
    UART_Process(test_handle);
    apply_uart_effects();
    assert(UART_DMA_TX->CNDTR == 3U);
    assert(memcmp(dma_tx_data(), &wrapped[1], 3U) == 0);
    finish_tx(false, 0U);

    const uint8_t six[] = {1U, 2U, 3U, 4U, 5U, 6U};
    assert(UART_Write(test_handle, six, sizeof six) == sizeof six);
    apply_uart_effects();
    finish_tx(true, 4U); /* 2 bytes reached TDR, 4 are explicitly dropped. */
    UART_GetStats(test_handle, &snapshot);
    assert(snapshot.tx_bytes == 261U && snapshot.tx_dropped == 151U);
    assert(snapshot.dma_errors == 1U);

    /* Recovery sends new traffic instead of replaying the ambiguous old span. */
    assert(UART_Write(test_handle, wrapped, 3U) == 3U);
    apply_uart_effects();
    assert(UART_DMA_TX->CNDTR == 3U);
    assert(memcmp(dma_tx_data(), wrapped, 3U) == 0);
    assert(UART_Write(test_handle, &wrapped[3], 1U) == 1U);
    apply_uart_effects();
    assert(UART_DMA_TX->CNDTR == 3U); /* Appending cannot reprogram active DMA. */
    finish_tx(false, 0U);
    UART_Process(test_handle);
    apply_uart_effects();
    assert(UART_DMA_TX->CNDTR == 1U && dma_tx_data()[0] == wrapped[3]);
    finish_tx(false, 0U);
    mock_uart.ISR |= UART_TC;
    assert(UART_TxIdle(test_handle));
}

static void run_lifecycle_tests(void)
{
    init_uart(UART_RX_DMA);
    const uint8_t tx[] = {0x11U, 0x22U, 0x33U};
    assert(UART_Write(test_handle, tx, sizeof tx) == sizeof tx);
    apply_uart_effects();
    dma_receive(0U, 5U, false);
    signal_idle();

    UART_Handle_t replacement = {0};
    replacement.pUSARTx = USART2;
    replacement.UART_Config = test_handle->UART_Config;
    replacement.UART_Config.UART_IRQPriority = 2U;
    uint32_t rx_count = UART_DMA_RX->CNDTR;
    uint32_t tx_count = UART_DMA_TX->CNDTR;
    uint32_t control = mock_uart.CR1;
    assert(!UART_Init(&replacement));
    assert(!UART_Init(test_handle)); /* Reinit cannot erase a live queue. */
    assert(!UART_DeInit(&replacement));
    assert(UART_DMA_RX->CNDTR == rx_count && UART_DMA_TX->CNDTR == tx_count);
    assert(mock_uart.CR1 == control && !replacement.Initialized);
    expect_rx(0U, 5U);

    /* Non-owner pointers are compared before any dereference/MMIO. */
    volatile uintptr_t non_owner_address = 1U;
    UART_Handle_t *wrong = (UART_Handle_t *)non_owner_address;
    uint8_t value = 0U;
    UART_Stats untouched = {.rx_bytes = 0x12345678U};
    assert(UART_Write(wrong, tx, sizeof tx) == 0U);
    assert(UART_Read(wrong, &value, 1U) == 0U);
    assert(!UART_TxIdle(wrong) && UART_RxEpoch(wrong) == 0U);
    assert(!UART_DeInit(wrong));
    UART_GetStats(wrong, &untouched);
    assert(untouched.rx_bytes == 0x12345678U);
    UART_Process(wrong);
    UART_DiscardRx(wrong);
    UART_IRQHandling(wrong);
    UART_DMARxIRQHandling(wrong);
    UART_DMATxIRQHandling(wrong);
    assert(UART_DMA_RX->CNDTR == rx_count && UART_DMA_TX->CNDTR == tx_count);
    assert(mock_uart.CR1 == control);

    UART_Handle_t *former_owner = test_handle;
    assert(UART_DeInit(former_owner));
    apply_uart_effects();
    assert(!former_owner->Initialized);
    assert(!mock_nvic_enabled[16] && !mock_nvic_enabled[17] && !mock_nvic_enabled[38]);
    assert((UART_DMA_RX->CCR & DMA_EN) == 0U && (UART_DMA_TX->CCR & DMA_EN) == 0U);
    assert(mock_uart.CR1 == 0U && mock_uart.CR3 == 0U);
    assert((mock_rcc.APB1ENR & (1U << 17)) == 0U);
    assert(former_owner->Stats.tx_dropped == sizeof tx); /* Explicit abort. */

    /* Late IRQ after DeInit must not touch even pending hardware flags. */
    UART_DMA_ISR = RX_DMA_HT | RX_DMA_TC | TX_DMA_TC;
    UART_DMA_IFCR = 0U;
    mock_uart.ISR = UART_RXNE | UART_IDLE | UART_ERRORS;
    UART_IRQHandling(former_owner);
    UART_DMARxIRQHandling(former_owner);
    UART_DMATxIRQHandling(former_owner);
    UART_Process(former_owner);
    assert(UART_DMA_IFCR == 0U && mock_uart.ICR == 0U);

    test_handle = &replacement;
    mock_uart.ISR = UART_ACKS;
    assert(UART_Init(test_handle));
    apply_uart_effects();
    assert(mock_nvic_priority[16] == 2U && mock_nvic_priority[17] == 2U &&
           mock_nvic_priority[38] == 2U);
    UART_Stats fresh;
    UART_GetStats(test_handle, &fresh);
    assert(fresh.rx_bytes == 0U && fresh.tx_bytes == 0U && fresh.tx_dropped == 0U);
    assert(!UART_DeInit(former_owner)); /* Old owner cannot stop the new owner. */
    dma_receive(100U, 4U, false);
    signal_idle();
    expect_rx(100U, 4U);
    assert(UART_DeInit(test_handle));
    apply_uart_effects();
    test_handle = &test_uart; /* replacement is about to leave scope. */
}

int main(void)
{
    void *mapped = map_dma_registers();
    uint8_t value = 0U;
    assert(UART_Write(test_handle, &value, 1U) == 0U);
    assert(UART_Read(test_handle, &value, 1U) == 0U);
    assert(!UART_TxIdle(test_handle));
    assert(!UART_Init(NULL));
    test_handle->pUSARTx = USART1; /* Unsupported instance rejected before MMIO. */
    assert(!UART_Init(test_handle));
    test_handle->pUSARTx = USART2;
    test_handle->UART_Config = (UART_Config_t){0U, 115200U, UART_RX_IRQ, 5U};
    assert(!UART_Init(test_handle));
    test_handle->UART_Config = (UART_Config_t){8000000U, 0U, UART_RX_IRQ, 5U};
    assert(!UART_Init(test_handle));
    test_handle->UART_Config = (UART_Config_t){8000000U, 600000U, UART_RX_IRQ, 5U};
    assert(!UART_Init(test_handle));
    test_handle->UART_Config = (UART_Config_t){8000000U, 115200U, (UART_RxMode)7, 5U};
    assert(!UART_Init(test_handle));
    test_handle->UART_Config = (UART_Config_t){8000000U, 115200U, UART_RX_IRQ, 16U};
    assert(!UART_Init(test_handle));

    UART_PeriClockControl(USART2, ENABLE);
    assert((mock_rcc.APB1ENR & (1U << 17)) != 0U);
    UART_PeriClockControl(USART1, DISABLE);
    assert((mock_rcc.APB1ENR & (1U << 17)) != 0U);
    UART_PeriClockControl(USART2, DISABLE);
    assert((mock_rcc.APB1ENR & (1U << 17)) == 0U);
    run_irq_rx_tests();
    run_dma_rx_tests();
    run_tx_tests();
    run_lifecycle_tests();

    /* A missing enable acknowledgement returns failure instead of hanging. */
    mock_uart.ISR = 0U;
    test_handle->UART_Config = (UART_Config_t){8000000U, 115200U, UART_RX_DMA, 5U};
    assert(!UART_Init(test_handle));
    assert((mock_uart.CR1 & UART_UE) == 0U);
    assert((mock_rcc.APB1ENR & (1U << 17)) == 0U);
    assert(!test_handle->Initialized);
    /* Failed Init retains no owner binding: a fresh handle can immediately own it. */
    UART_Handle_t after_failure = {0};
    after_failure.pUSARTx = USART2;
    after_failure.UART_Config = test_handle->UART_Config;
    mock_uart.ISR = UART_ACKS;
    assert(UART_Init(&after_failure));
    apply_uart_effects();
    assert(UART_DeInit(&after_failure));
    apply_uart_effects();
    assert(mock_irq_mask == 0U);
    unmap_dma_registers(mapped);
    puts("UART register-mock tests passed");
    return 0;
}
