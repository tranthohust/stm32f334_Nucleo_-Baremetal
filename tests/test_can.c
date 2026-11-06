/* Register mock tests: the real driver runs with RAM-backed registers.
 * Hardware W1C/FIFO advancement are not emulated automatically: each test
 * injects the next register snapshot explicitly, like a peripheral event.
 */
#include "../src/drivers/STM32F334_CAN.h"
#include "../src/drivers/STM32F334_CORE.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

static uint32_t mock_can_words[0x400U / sizeof(uint32_t)];
static RCC_RegDef_t mock_rcc;
static uint32_t mock_ms;
static uint32_t mock_irq_mask;
static bool advancing_ticks;
static unsigned ack_behavior; /* 0=follow INRQ, 1=never acknowledge, 2=always. */
static uint32_t enabled_irqs;
static uint8_t priorities[4];
static CAN_Handle_t hcan;

#undef CAN1_BASEADDR
#undef RCC
#define CAN1_BASEADDR ((uintptr_t)mock_can_words)
#define RCC (&mock_rcc)

uint32_t IRQ_Save(void)
{
    uint32_t saved = mock_irq_mask;
    mock_irq_mask = 1U;
    return saved;
}
void IRQ_Restore(uint32_t saved) { mock_irq_mask = saved; }
void NVIC_EnableIRQ(uint8_t irq)
{
    assert(irq >= 19U && irq <= 22U);
    enabled_irqs |= 1U << irq;
}
void NVIC_DisableIRQ(uint8_t irq)
{
    assert(irq >= 19U && irq <= 22U);
    enabled_irqs &= ~(1U << irq);
}
void NVIC_ClearPendingIRQ(uint8_t irq) { assert(irq >= 19U && irq <= 22U); }
void NVIC_SetPriority(uint8_t irq, uint8_t priority)
{
    assert(irq >= 19U && irq <= 22U && priority < 16U);
    priorities[irq - 19U] = priority;
}
#include "../src/drivers/STM32F334_CAN.c"

#define CAN_HW CAN1

uint32_t Timebase_Millis(void)
{
    bool ack = ack_behavior == 2U ||
               (ack_behavior == 0U && (CAN_HW->MCR & CAN_MCR_INRQ) != 0U);
    if (ack) CAN_HW->MSR |= CAN_MSR_INAK;
    else CAN_HW->MSR &= ~CAN_MSR_INAK;
    uint32_t now = mock_ms;
    if (advancing_ticks) ++mock_ms;
    return now;
}

#define TME_ALL ((1U << 26) | (1U << 27) | (1U << 28))

static CAN_Frame frame(uint32_t id)
{
    CAN_Frame f = {
        .id = id, .dlc = 4U, .extended = false, .remote = false,
        .data = {0x11U, 0x22U, 0x33U, 0x44U, 0xEEU, 0xEEU, 0xEEU, 0xEEU}
    };
    return f;
}

static void fresh_driver(void)
{
    /* Reset RAM-backed peripheral + owner = mot lan reset MCU trong test. */
    bound_handle = NULL;
    memset(&hcan, 0, sizeof hcan);
    hcan.pCANx = CAN1;
    hcan.CAN_Config = (CAN_Config_t){
        .CAN_PCLKHz = 8000000U, .CAN_BitRate = 125000U,
        .CAN_Mode = CAN_MODE_SILENT_LOOPBACK, .CAN_IRQPriority = 6U,
        .CAN_TxTimeoutMs = 100U
    };
    memset(mock_can_words, 0, sizeof mock_can_words);
    memset(&mock_rcc, 0, sizeof mock_rcc);
    mock_ms = 0U;
    mock_irq_mask = 0U;
    ack_behavior = 0U;
    advancing_ticks = false;
    assert(CAN_Init(&hcan));
    CAN_HW->TSR = TME_ALL;
}

static void complete_mailbox(unsigned mailbox, uint32_t flags)
{
    CAN_HW->TSR = TME_ALL | (flags << (mailbox * 8U));
    CAN_TXIRQHandling(&hcan);
    /* A real W1C write preserves TME. Restore this property before next call. */
    CAN_HW->TSR = TME_ALL;
}

static void inject_rx(unsigned fifo, uint32_t id, uint8_t dlc,
                      uint32_t low, uint32_t high)
{
    CAN_HW->RX[fifo].ID = id;
    CAN_HW->RX[fifo].LENGTH = 0xABCD0000U | dlc;
    CAN_HW->RX[fifo].LOW = low;
    CAN_HW->RX[fifo].HIGH = high;
    if (fifo == 0U) {
        CAN_HW->RF0R = 1U;
        CAN_RX0IRQHandling(&hcan);
        assert(CAN_HW->RF0R == CAN_RFR_RELEASE);
        CAN_HW->RF0R = 0U;
    } else {
        CAN_HW->RF1R = 1U;
        CAN_RX1IRQHandling(&hcan);
        assert(CAN_HW->RF1R == CAN_RFR_RELEASE);
        CAN_HW->RF1R = 0U;
    }
}

static void test_init_and_timing(void)
{
    uint32_t btr;
    assert(!bit_timing(0U, 125000U, &btr));
    assert(!bit_timing(72000000U, 125000U, &btr));
    assert(!bit_timing(8000000U, 333333U, &btr));
    const uint32_t rates[] = {125000U, 250000U, 500000U, 1000000U};
    for (unsigned i = 0U; i < sizeof rates / sizeof rates[0]; ++i) {
        assert(bit_timing(8000000U, rates[i], &btr));
        uint32_t brp = (btr & 0x3FFU) + 1U;
        uint32_t bs1 = ((btr >> 16) & 15U) + 1U;
        uint32_t bs2 = ((btr >> 20) & 7U) + 1U;
        assert(8000000U / (brp * (1U + bs1 + bs2)) == rates[i]);
    }
    fresh_driver();
    assert((CAN_HW->BTR & (3U << 30)) == (3U << 30)); /* silent loopback. */
    assert((CAN_HW->MCR & CAN_MCR_INRQ) == 0U);
    assert((CAN_HW->MCR & CAN_MCR_ABOM) != 0U);
    assert((CAN_HW->MCR & (1U << 4)) == 0U); /* Automatic retry enabled. */
    assert(CAN_HW->FILTER[0].ID == 0U && CAN_HW->FILTER[0].MASK == 0U);
    assert(CAN_HW->FA1R == 1U && CAN_HW->FS1R == 1U);
    assert(CAN_HW->FM1R == 0U && CAN_HW->FFA1R == 0U && CAN_HW->FMR == 0U);
    assert(enabled_irqs == (15U << 19));

    for (unsigned i = 0U; i < 4U; ++i) assert(priorities[i] == 6U);
    assert(CAN_DeInit(&hcan));

    /* Both INAK entry and exit have bounded failure paths; failure releases owner. */
    advancing_ticks = true;
    ack_behavior = 1U;
    assert(!CAN_Init(&hcan));
    assert(hcan.counters.init_timeouts == 1U && !hcan.initialized && enabled_irqs == 0U);
    assert(bound_handle == NULL && (RCC->APB1ENR & (1U << 25)) == 0U);
    ack_behavior = 2U;
    assert(!CAN_Init(&hcan));
    assert(hcan.counters.init_timeouts == 1U && !hcan.initialized);
    assert(bound_handle == NULL);
    ack_behavior = 0U;
    advancing_ticks = false;
    assert(CAN_Init(&hcan)); /* Rebind succeeds after either failed init. */
}

static void test_tx_packing_and_validation(void)
{
    fresh_driver();
    CAN_Frame f = frame(0x123U);
    assert(!CAN_Send(&hcan, NULL));
    CAN_Frame invalid = f;
    invalid.id = 0x800U;
    assert(!CAN_Send(&hcan, &invalid));
    invalid = f;
    invalid.dlc = 9U;
    assert(!CAN_Send(&hcan, &invalid));
    assert(CAN_Send(&hcan, &f));
    assert(CAN_HW->TX[0].ID == ((0x123U << 21) | CAN_TX_REQUEST));
    assert(CAN_HW->TX[0].LENGTH == 4U);
    assert(CAN_HW->TX[0].LOW == 0x44332211U && CAN_HW->TX[0].HIGH == 0U);
    f.data[0] = 0U; /* Send copied data; caller buffer no longer owned. */
    assert(hcan.inflight[0].frame.data[0] == 0x11U);

    CAN_Frame remote = frame(0x1ABCDEU);
    remote.extended = true;
    remote.remote = true;
    remote.dlc = 8U;
    assert(CAN_Send(&hcan, &remote));
    assert(CAN_HW->TX[1].ID == ((remote.id << 3) | 7U));
    assert(CAN_HW->TX[1].LOW == 0U && CAN_HW->TX[1].HIGH == 0U);
    remote.id = 0x20000000U;
    assert(!CAN_Send(&hcan, &remote));

    complete_mailbox(0U, 3U); /* RQCP + TXOK. */
    CAN_TxResult result;
    assert(CAN_GetTxResult(&hcan, &result));
    assert(result.status == CAN_TX_OK && result.frame.id == 0x123U);
    assert(result.frame.data[0] == 0x11U);
    complete_mailbox(1U, 5U); /* RQCP + arbitration loss. */
    assert(CAN_GetTxResult(&hcan, &result) && result.status == CAN_TX_ARBITRATION_LOST);
    assert(!CAN_GetTxResult(&hcan, &result));
}

static void test_rx_decode_and_overflow(void)
{
    fresh_driver();
    CAN_Frame output;
    inject_rx(0U, 0x321U << 21, 6U, 0x44332211U, 0x88776655U);
    assert(CAN_Receive(&hcan, &output));
    assert(output.id == 0x321U && !output.extended && !output.remote);
    assert(output.dlc == 6U && output.data[0] == 0x11U && output.data[5] == 0x66U);
    assert(output.data[6] == 0U && output.data[7] == 0U);
    inject_rx(1U, (0x1ABCDEU << 3) | 6U, 8U, 0xFFFFFFFFU, 0xFFFFFFFFU);
    assert(CAN_Receive(&hcan, &output));
    assert(output.id == 0x1ABCDEU && output.extended && output.remote);
    for (unsigned i = 0U; i < 8U; ++i) assert(output.data[i] == 0U);
    inject_rx(0U, 0U, 9U, 0U, 0U);
    assert(!CAN_Receive(&hcan, &output) && hcan.counters.rx_invalid_dlc == 1U);

    for (unsigned i = 0U; i < CAN_RX_QUEUE_SIZE + 1U; ++i)
        inject_rx(i % 2U, i << 21, 0U, 0U, 0U);
    assert(hcan.counters.rx_queue_overflow == 1U);
    for (unsigned i = 0U; i < CAN_RX_QUEUE_SIZE; ++i) {
        assert(CAN_Receive(&hcan, &output) && output.id == i); /* Preserve oldest. */
    }
    assert(!CAN_Receive(&hcan, &output));
    CAN_HW->RF0R = CAN_RFR_OVERRUN;
    CAN_RX0IRQHandling(&hcan);
    assert(hcan.counters.rx_fifo_overflow == 1U);
}

static void test_queue_mailbox_ownership_and_timeout(void)
{
    fresh_driver();
    CAN_Frame f = frame(1U);
    for (unsigned i = 0U; i < 3U + CAN_TX_QUEUE_SIZE; ++i) {
        f.id = i;
        assert(CAN_Send(&hcan, &f));
    }
    assert(!CAN_Send(&hcan, &f) && hcan.counters.tx_queue_full == 1U);
    assert(hcan.tx_count == CAN_TX_QUEUE_SIZE);
    /* Hardware may report TME before software collected RQCP: do not reuse. */
    CAN_HW->TSR = TME_ALL;
    CAN_Process(&hcan);
    assert(hcan.inflight[0].active && hcan.inflight[0].frame.id == 0U);
    assert(hcan.tx_count == CAN_TX_QUEUE_SIZE);
    complete_mailbox(0U, 3U);
    CAN_Process(&hcan);
    assert(hcan.inflight[0].active && hcan.inflight[0].frame.id == 3U);
    assert(hcan.tx_count == CAN_TX_QUEUE_SIZE - 1U);

    fresh_driver();
    mock_ms = 0xFFFFFFF0U;
    assert(CAN_Send(&hcan, &f));
    mock_ms += hcan.CAN_Config.CAN_TxTimeoutMs;
    CAN_HW->TSR = TME_ALL;
    CAN_Process(&hcan);
    assert(hcan.inflight[0].active && hcan.inflight[0].abort_requested);
    assert(CAN_HW->TSR == (1U << 7) && hcan.counters.tx_timeouts == 1U);
    CAN_TxResult result;
    assert(!CAN_GetTxResult(&hcan, &result));
    CAN_HW->TSR = TME_ALL;
    CAN_Process(&hcan);
    assert(hcan.counters.tx_timeouts == 1U); /* Abort once; wait for RQCP. */
    complete_mailbox(0U, 1U);
    assert(CAN_GetTxResult(&hcan, &result) && result.status == CAN_TX_TIMEOUT);
    assert(!hcan.inflight[0].active);
}

static void test_busoff_and_result_overflow(void)
{
    fresh_driver();
    CAN_Frame f = frame(0x123U);
    CAN_HW->TSR = 1U << 26; /* Only mailbox0 available in this scenario. */
    assert(CAN_Send(&hcan, &f));
    assert(CAN_Send(&hcan, &f)); /* One remains queued. */
    CAN_HW->ESR = CAN_ESR_BOFF;
    CAN_SCEIRQHandling(&hcan);
    assert(hcan.counters.bus_off_events == 1U && hcan.counters.bus_off);
    assert(hcan.inflight[0].abort_requested && hcan.inflight[0].abort_reason == CAN_TX_BUS_OFF);
    assert(CAN_HW->TSR == CAN_ALL_ABORT && hcan.tx_count == 1U);
    CAN_HW->TSR = TME_ALL;
    CAN_Process(&hcan);
    assert(hcan.tx_count == 1U); /* No new traffic until recovery. */
    complete_mailbox(0U, 1U);
    CAN_TxResult result;
    assert(CAN_GetTxResult(&hcan, &result) && result.status == CAN_TX_BUS_OFF);
    CAN_HW->ESR = 0U;
    CAN_Process(&hcan);
    assert(!hcan.counters.bus_off && hcan.counters.bus_off_recoveries == 1U);
    assert(hcan.tx_count == 0U && hcan.inflight[0].active);
    complete_mailbox(0U, 3U);
    assert(CAN_GetTxResult(&hcan, &result) && result.status == CAN_TX_OK);

    fresh_driver();
    for (unsigned i = 0U; i < CAN_RESULT_QUEUE_SIZE + 1U; ++i) {
        f.id = i;
        assert(CAN_Send(&hcan, &f));
        complete_mailbox(0U, 3U);
    }
    assert(hcan.counters.tx_completed == CAN_RESULT_QUEUE_SIZE + 1U);
    assert(hcan.counters.tx_result_overflow == 1U);
    for (unsigned i = 0U; i < CAN_RESULT_QUEUE_SIZE; ++i)
        assert(CAN_GetTxResult(&hcan, &result) && result.frame.id == i);
    assert(!CAN_GetTxResult(&hcan, &result));
}

static void test_handle_ownership_and_deinit(void)
{
    fresh_driver();
    CAN_Handle_t other = {0};
    other.pCANx = CAN1;
    other.CAN_Config = hcan.CAN_Config;
    uint32_t old_btr = CAN_HW->BTR;
    uint32_t old_ier = CAN_HW->IER;
    assert(!CAN_Init(&hcan)); /* Khong reset tham cung mot owner dang chay. */
    assert(!CAN_Init(&other));
    assert(bound_handle == &hcan && hcan.initialized && !other.initialized);
    assert(CAN_HW->BTR == old_btr && CAN_HW->IER == old_ier);
    CAN_Frame f = frame(0x77U);
    CAN_TxResult result;
    assert(!CAN_Send(&other, &f) && !CAN_Receive(&other, &f));
    assert(!CAN_GetTxResult(&other, &result) && !CAN_DeInit(&other));
    CAN_Process(&other);
    CAN_TXIRQHandling(&other);
    CAN_RX0IRQHandling(&other);
    CAN_RX1IRQHandling(&other);
    CAN_SCEIRQHandling(&other);
    assert(CAN_HW->IER == old_ier && other.counters.error_interrupts == 0U);
    assert(!CAN_Init(NULL) && !CAN_DeInit(NULL));
    assert(!CAN_Send(NULL, &f) && !CAN_Receive(NULL, &f));
    assert(!CAN_GetTxResult(NULL, &result));
    CAN_Process(NULL);
    CAN_TXIRQHandling(NULL);
    CAN_RX0IRQHandling(NULL);
    CAN_RX1IRQHandling(NULL);
    CAN_SCEIRQHandling(NULL);

    /* Active TX va TX queued deu giu binding cho den khi hoan tat. */
    assert(CAN_Send(&hcan, &f));
    assert(!CAN_DeInit(&hcan));
    CAN_HW->TSR = 0U;
    assert(CAN_Send(&hcan, &f));
    assert(hcan.tx_count == 1U && !CAN_DeInit(&hcan));
    complete_mailbox(0U, 3U);
    CAN_Process(&hcan);
    assert(hcan.inflight[0].active && !CAN_DeInit(&hcan));
    complete_mailbox(0U, 3U);
    inject_rx(0U, 0x123U << 21, 0U, 0U, 0U);
    assert(hcan.rx_count == 1U && hcan.result_count == 2U);
    assert(CAN_DeInit(&hcan)); /* Idle: unread RX/results may be discarded. */
    assert(!hcan.initialized && bound_handle == NULL && enabled_irqs == 0U);
    assert(hcan.rx_count == 0U && hcan.result_count == 0U);
    assert((RCC->APB1ENR & (1U << 25)) == 0U);
    assert(!CAN_Send(&hcan, &f) && !CAN_DeInit(&hcan));

    other.CAN_Config.CAN_IRQPriority = 3U;
    assert(CAN_Init(&other));
    assert(bound_handle == &other && other.initialized);
    for (unsigned i = 0U; i < 4U; ++i) assert(priorities[i] == 3U);
    assert(CAN_DeInit(&other));
    assert(CAN_Init(&hcan));
}

static void test_config_validation_and_clock(void)
{
    fresh_driver();
    assert(CAN_DeInit(&hcan));
    CAN_Handle_t invalid = hcan;
    uint32_t clock_before = RCC->APB1ENR;
    invalid.pCANx = (CAN_RegDef_t *)(uintptr_t)0x1234U;
    assert(!CAN_Init(&invalid));
    CAN_PeriClockControl(invalid.pCANx, ENABLE);
    assert(RCC->APB1ENR == clock_before && bound_handle == NULL);
    invalid = hcan;
    invalid.CAN_Config.CAN_Mode = 2U;
    assert(!CAN_Init(&invalid));
    invalid = hcan;
    invalid.CAN_Config.CAN_IRQPriority = 16U;
    assert(!CAN_Init(&invalid));
    invalid = hcan;
    invalid.CAN_Config.CAN_TxTimeoutMs = 0U;
    assert(!CAN_Init(&invalid));
    invalid.CAN_Config.CAN_TxTimeoutMs = 0x80000000UL;
    assert(!CAN_Init(&invalid));
    invalid = hcan;
    invalid.CAN_Config.CAN_BitRate = 333333U;
    assert(!CAN_Init(&invalid));
    assert(bound_handle == NULL && RCC->APB1ENR == clock_before);
    CAN_PeriClockControl(CAN1, ENABLE);
    assert((RCC->APB1ENR & (1U << 25)) != 0U);
    CAN_PeriClockControl(CAN1, DISABLE);
    assert((RCC->APB1ENR & (1U << 25)) == 0U);

    hcan.CAN_Config.CAN_Mode = CAN_MODE_NORMAL;
    assert(CAN_Init(&hcan));
    assert((CAN_HW->BTR & (3U << 30)) == 0U);
    CAN_Stats stats;
    CAN_GetStats(&hcan, &stats);
    assert(stats.bit_timing == CAN_HW->BTR);
    CAN_GetStats(NULL, &stats);
    assert(stats.tx_queued == 0U && stats.bit_timing == 0U);
    CAN_GetStats(&hcan, NULL);
    assert(CAN_DeInit(&hcan));
}

int main(void)
{
    test_init_and_timing();
    test_tx_packing_and_validation();
    test_rx_decode_and_overflow();
    test_queue_mailbox_ownership_and_timeout();
    test_busoff_and_result_overflow();
    test_handle_ownership_and_deinit();
    test_config_validation_and_clock();
    assert(mock_irq_mask == 0U);
    puts("CAN register-mock tests passed");
    return 0;
}
