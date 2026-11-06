#include "STM32F334_CAN.h"
#include "STM32F334_CORE.h"
#include <stddef.h>

/* Nguon doi chieu: ST RM0364, bxCAN; CMSIS stm32f334x8.h.
 * https://www.st.com/resource/en/reference_manual/dm00093941.pdf
 */
_Static_assert(offsetof(CAN_RegDef_t, TX) == 0x180U, "CAN TX offset");
_Static_assert(offsetof(CAN_RegDef_t, RX) == 0x1B0U, "CAN RX offset");
_Static_assert(offsetof(CAN_RegDef_t, FMR) == 0x200U, "CAN FMR offset");
_Static_assert(offsetof(CAN_RegDef_t, FILTER) == 0x240U, "CAN filter offset");

#define CAN_IRQ_TX 19U
#define CAN_IRQ_RX0 20U
#define CAN_IRQ_RX1 21U
#define CAN_IRQ_SCE 22U
#define CAN_MCR_INRQ (1UL << 0)
#define CAN_MCR_TXFP (1UL << 2)
#define CAN_MCR_RFLM (1UL << 3)
#define CAN_MCR_ABOM (1UL << 6)
#define CAN_MSR_INAK (1UL << 0)
#define CAN_ESR_BOFF (1UL << 2)
#define CAN_RFR_PENDING 3UL
#define CAN_RFR_FULL (1UL << 3)
#define CAN_RFR_OVERRUN (1UL << 4)
#define CAN_RFR_RELEASE (1UL << 5)
#define CAN_IDENTIFIER_EXT (1UL << 2)
#define CAN_IDENTIFIER_REMOTE (1UL << 1)
#define CAN_TX_REQUEST (1UL << 0)
#define CAN_ALL_ABORT ((1UL << 7) | (1UL << 15) | (1UL << 23))
#define CAN_ALL_COMPLETED ((1UL << 0) | (1UL << 8) | (1UL << 16))
#define CAN_INIT_TIMEOUT_MS 100U
#define CAN_INIT_MAX_POLLS 2000000UL

/* Chi con binding cua peripheral la static; moi du lieu nam trong handle. */
static CAN_Handle_t *bound_handle;

static bool valid_handle(const CAN_Handle_t *pCANHandle)
{
    return pCANHandle != NULL && pCANHandle == bound_handle &&
           pCANHandle->pCANx == CAN1 && pCANHandle->initialized;
}

/* Moi queue/state deu duoc bao ve boi IRQ_Save, ke ca hai RX ISR cung push.
 * Khong callback, parse, printf hay cho flag trong ISR.
 */
static bool valid_frame(const CAN_Frame *frame)
{
    return frame != NULL && frame->dlc <= 8U &&
           frame->id <= (frame->extended ? 0x1FFFFFFFUL : 0x7FFUL);
}

static CAN_Frame clean_frame(const CAN_Frame *source)
{
    CAN_Frame frame = {0};
    frame.id = source->id;
    frame.dlc = source->dlc;
    frame.extended = source->extended;
    frame.remote = source->remote;
    for (uint8_t i = 0; !frame.remote && i < frame.dlc; ++i) {
        frame.data[i] = source->data[i];
    }
    return frame;
}

static bool bit_timing(uint32_t clock, uint32_t bitrate, uint32_t *btr)
{
    /* bitrate = PCLK / (BRP * (1 + BS1 + BS2)); SJW=1 TQ.
     * Tim nghiem nguyen, uu tien sample point gan 87.5%, sau do TQ lon.
     * Tai 8MHz/500k: BRP=1, BS1=13, BS2=2, tong=16 TQ.
     */
    if (clock == 0U || clock > 36000000UL || bitrate < 10000U ||
        bitrate > 1000000UL || clock % bitrate != 0U) {
        return false;
    }
    uint32_t divider = clock / bitrate;
    uint32_t best_score = 1001U;
    bool found = false;
    for (uint32_t tq = 25U; tq >= 8U; --tq) {
        if (divider % tq != 0U) {
            continue;
        }
        uint32_t prescaler = divider / tq;
        if (prescaler < 1U || prescaler > 1024U) {
            continue;
        }
        uint32_t bs2 = (tq + 4U) / 8U;
        if (bs2 == 0U) {
            bs2 = 1U;
        }
        uint32_t bs1 = tq - 1U - bs2;
        if (bs1 > 16U) {
            bs1 = 16U;
            bs2 = tq - 17U;
        }
        if (bs2 > 8U) {
            continue;
        }
        uint32_t sample = ((1U + bs1) * 1000U) / tq;
        uint32_t score = sample > 875U ? sample - 875U : 875U - sample;
        if (score < best_score) {
            best_score = score;
            *btr = (prescaler - 1U) | ((bs1 - 1U) << 16) |
                   ((bs2 - 1U) << 20);
            found = true;
        }
    }
    return found;
}

static bool wait_init_ack(CAN_Handle_t *pCANHandle, bool expected)
{
    uint32_t started = Timebase_Millis();
    for (uint32_t polls = 0U; polls < CAN_INIT_MAX_POLLS; ++polls) {
        if (((pCANHandle->pCANx->MSR & CAN_MSR_INAK) != 0U) == expected) {
            return true;
        }
        if ((uint32_t)(Timebase_Millis() - started) >= CAN_INIT_TIMEOUT_MS) {
            return false;
        }
    }
    /* Bounded polling van thoat duoc neu SysTick chua chay/bi mask. */
    return false;
}

static void publish_result(CAN_Handle_t *pCANHandle, const CAN_InFlight *mailbox, CAN_TxStatus status,
                           uint32_t esr)
{
    if (status == CAN_TX_OK) {
        ++pCANHandle->counters.tx_completed;
    } else {
        ++pCANHandle->counters.tx_failed;
    }
    if (pCANHandle->result_count == CAN_RESULT_QUEUE_SIZE) {
        ++pCANHandle->counters.tx_result_overflow;
        return;
    }
    pCANHandle->result_queue[pCANHandle->result_head].frame = mailbox->frame;
    pCANHandle->result_queue[pCANHandle->result_head].status = status;
    pCANHandle->result_queue[pCANHandle->result_head].error_status = esr;
    pCANHandle->result_head = (uint8_t)((pCANHandle->result_head + 1U) % CAN_RESULT_QUEUE_SIZE);
    ++pCANHandle->result_count;
}

static void collect_completions(CAN_Handle_t *pCANHandle)
{
    uint32_t tsr = pCANHandle->pCANx->TSR;
    uint32_t esr = pCANHandle->pCANx->ESR;
    for (uint8_t i = 0; i < 3U; ++i) {
        uint32_t shift = (uint32_t)i * 8U;
        uint32_t flags = (tsr >> shift) & 0xFU;
        if ((flags & 1U) == 0U) {
            continue;
        }
        /* RQCP la W1C: ghi dung mask, khong dung read-modify-write. */
        pCANHandle->pCANx->TSR = 1UL << shift;
        if (!pCANHandle->inflight[i].active) {
            continue;
        }
        CAN_TxStatus status;
        if ((flags & 2U) != 0U) {
            status = CAN_TX_OK;
        } else if (pCANHandle->inflight[i].abort_requested) {
            status = pCANHandle->inflight[i].abort_reason;
        } else if ((flags & 8U) != 0U) {
            status = CAN_TX_ERROR;
        } else if ((flags & 4U) != 0U) {
            status = CAN_TX_ARBITRATION_LOST;
        } else {
            status = CAN_TX_ABORTED;
        }
        publish_result(pCANHandle, &pCANHandle->inflight[i], status, esr);
        pCANHandle->inflight[i].active = false;
        pCANHandle->inflight[i].abort_requested = false;
    }
}

static void sample_bus_state(CAN_Handle_t *pCANHandle)
{
    uint32_t esr = pCANHandle->pCANx->ESR;
    bool off = (esr & CAN_ESR_BOFF) != 0U;
    if (off && !pCANHandle->counters.bus_off) {
        ++pCANHandle->counters.bus_off_events;
        /* Frame da vao hardware se bi abort va bao result BUS_OFF.
         * Frame con trong software queue duoc giu, khong tu xoa/retry frame loi.
         * ABOM cho hardware tu cho bus recessive theo CAN, Process theo doi ESR.
         */
        for (uint8_t i = 0; i < 3U; ++i) {
            if (pCANHandle->inflight[i].active && !pCANHandle->inflight[i].abort_requested) {
                pCANHandle->inflight[i].abort_requested = true;
                pCANHandle->inflight[i].abort_reason = CAN_TX_BUS_OFF;
            }
        }
        pCANHandle->pCANx->TSR = CAN_ALL_ABORT;
    } else if (!off && pCANHandle->counters.bus_off) {
        ++pCANHandle->counters.bus_off_recoveries;
    }
    pCANHandle->counters.bus_off = off;
    pCANHandle->counters.last_error_status = esr;
}

static void schedule_tx(CAN_Handle_t *pCANHandle)
{
    if (pCANHandle->counters.bus_off) {
        return;
    }
    for (uint8_t i = 0; i < 3U && pCANHandle->tx_count != 0U; ++i) {
        if (pCANHandle->inflight[i].active || (pCANHandle->pCANx->TSR & (1UL << (26U + i))) == 0U) {
            continue;
        }
        CAN_Frame frame = pCANHandle->tx_queue[pCANHandle->tx_tail];
        pCANHandle->tx_tail = (uint8_t)((pCANHandle->tx_tail + 1U) % CAN_TX_QUEUE_SIZE);
        --pCANHandle->tx_count;
        uint32_t low = 0U, high = 0U;
        for (uint8_t byte = 0; byte < 4U; ++byte) {
            low |= (uint32_t)frame.data[byte] << ((uint32_t)byte * 8U);
            high |= (uint32_t)frame.data[byte + 4U] << ((uint32_t)byte * 8U);
        }
        uint32_t identifier = frame.extended
                                  ? (frame.id << 3) | CAN_IDENTIFIER_EXT
                                  : frame.id << 21;
        if (frame.remote) {
            identifier |= CAN_IDENTIFIER_REMOTE;
        }
        pCANHandle->inflight[i].frame = frame;
        pCANHandle->inflight[i].started_ms = Timebase_Millis();
        pCANHandle->inflight[i].active = true;
        pCANHandle->inflight[i].abort_requested = false;
        pCANHandle->pCANx->TX[i].ID = identifier;
        pCANHandle->pCANx->TX[i].LENGTH = frame.dlc;
        pCANHandle->pCANx->TX[i].LOW = low;
        pCANHandle->pCANx->TX[i].HIGH = high;
        /* TXRQ cuoi cung: den day moi cho hardware quyen doc mailbox. */
        pCANHandle->pCANx->TX[i].ID = identifier | CAN_TX_REQUEST;
    }
}

static void drain_rx(CAN_Handle_t *pCANHandle, uint8_t fifo)
{
    volatile uint32_t *rfr = fifo == 0U ? &pCANHandle->pCANx->RF0R : &pCANHandle->pCANx->RF1R;
    if ((*rfr & CAN_RFR_OVERRUN) != 0U) {
        ++pCANHandle->counters.rx_fifo_overflow;
        *rfr = CAN_RFR_OVERRUN;
    }
    /* FIFO hardware sau 3 frame; gioi han 3 lan de ISR khong chiem CPU vo han. */
    for (uint8_t n = 0; n < 3U && (*rfr & CAN_RFR_PENDING) != 0U; ++n) {
        uint32_t id = pCANHandle->pCANx->RX[fifo].ID;
        uint32_t dlc = pCANHandle->pCANx->RX[fifo].LENGTH & 0xFU;
        uint32_t low = pCANHandle->pCANx->RX[fifo].LOW;
        uint32_t high = pCANHandle->pCANx->RX[fifo].HIGH;
        CAN_Frame frame = {0};
        frame.extended = (id & CAN_IDENTIFIER_EXT) != 0U;
        frame.remote = (id & CAN_IDENTIFIER_REMOTE) != 0U;
        frame.id = frame.extended ? (id >> 3) & 0x1FFFFFFFUL : id >> 21;
        frame.dlc = (uint8_t)dlc;
        if (dlc <= 8U) {
            for (uint8_t byte = 0; !frame.remote && byte < frame.dlc; ++byte) {
                uint32_t word = byte < 4U ? low : high;
                frame.data[byte] = (uint8_t)(word >> (((uint32_t)byte % 4U) * 8U));
            }
            if (pCANHandle->rx_count < CAN_RX_QUEUE_SIZE) {
                pCANHandle->rx_queue[pCANHandle->rx_head] = frame;
                pCANHandle->rx_head = (uint8_t)((pCANHandle->rx_head + 1U) % CAN_RX_QUEUE_SIZE);
                ++pCANHandle->rx_count;
                ++pCANHandle->counters.rx_frames;
            } else {
                /* Bo frame MOI, giu nguyen cac frame cu chua doc. */
                ++pCANHandle->counters.rx_queue_overflow;
            }
        } else {
            ++pCANHandle->counters.rx_invalid_dlc;
        }
        /* Da copy du frame moi release; direct write tranh xoa co W1C khac. */
        *rfr = CAN_RFR_RELEASE;
    }
    if ((*rfr & CAN_RFR_FULL) != 0U) {
        *rfr = CAN_RFR_FULL;
    }
}

void CAN_PeriClockControl(CAN_RegDef_t *pCANx, uint8_t EnorDi)
{
    if (pCANx != CAN1) {
        return;
    }
    uint32_t key = IRQ_Save();
    if (EnorDi == ENABLE) {
        CAN1_PCLK_EN();
        (void)RCC->APB1ENR;
    } else {
        CAN1_PCLK_DI();
    }
    IRQ_Restore(key);
}

static bool init_failed(CAN_Handle_t *pCANHandle)
{
    uint32_t key = IRQ_Save();
    pCANHandle->pCANx->IER = 0U;
    CAN1_REG_RESET();
    CAN_PeriClockControl(pCANHandle->pCANx, DISABLE);
    ++pCANHandle->counters.init_timeouts;
    pCANHandle->initialized = false;
    bound_handle = NULL;
    IRQ_Restore(key);
    return false;
}

bool CAN_Init(CAN_Handle_t *pCANHandle)
{
    uint32_t btr = 0U;
    if (pCANHandle == NULL || pCANHandle->pCANx != CAN1 ||
        pCANHandle->CAN_Config.CAN_Mode > CAN_MODE_SILENT_LOOPBACK ||
        pCANHandle->CAN_Config.CAN_IRQPriority > 15U ||
        pCANHandle->CAN_Config.CAN_TxTimeoutMs == 0U ||
        pCANHandle->CAN_Config.CAN_TxTimeoutMs > 0x7FFFFFFFUL ||
        !bit_timing(pCANHandle->CAN_Config.CAN_PCLKHz,
                    pCANHandle->CAN_Config.CAN_BitRate, &btr)) {
        return false;
    }
    uint32_t key = IRQ_Save();
    /* Ke ca cung handle cung phai DeInit: khong reset tham queue dang chay. */
    if (bound_handle != NULL) {
        IRQ_Restore(key);
        return false;
    }
    bound_handle = pCANHandle;
    pCANHandle->initialized = false;
    for (uint8_t irq = CAN_IRQ_TX; irq <= CAN_IRQ_SCE; ++irq) {
        NVIC_DisableIRQ(irq);
        NVIC_ClearPendingIRQ(irq);
    }
    pCANHandle->rx_head = pCANHandle->rx_tail = pCANHandle->rx_count = 0U;
    pCANHandle->tx_head = pCANHandle->tx_tail = pCANHandle->tx_count = 0U;
    pCANHandle->result_head = pCANHandle->result_tail = pCANHandle->result_count = 0U;
    pCANHandle->counters = (CAN_Stats){0};
    for (uint8_t i = 0; i < 3U; ++i) {
        pCANHandle->inflight[i].active = false;
        pCANHandle->inflight[i].abort_requested = false;
    }
    CAN_PeriClockControl(pCANHandle->pCANx, ENABLE);
    CAN1_REG_RESET();
    pCANHandle->pCANx->IER = 0U;
    IRQ_Restore(key);

    /* Board setup GPIO truoc khi goi ham nay; driver chi quan ly CAN.
     * TXFP giu thu tu gui; RFLM giu frame cu khi hardware FIFO tran.
     * NART=0 de hardware tu retransmit, ABOM tu phuc hoi sau bus-off.
     */
    pCANHandle->pCANx->MCR = CAN_MCR_INRQ | CAN_MCR_TXFP | CAN_MCR_RFLM | CAN_MCR_ABOM;
    if (!wait_init_ack(pCANHandle, true)) {
        return init_failed(pCANHandle);
    }
    if (pCANHandle->CAN_Config.CAN_Mode == CAN_MODE_SILENT_LOOPBACK) {
        btr |= (1UL << 30) | (1UL << 31); /* LBKM + SILM */
    }
    pCANHandle->pCANx->BTR = btr;
    pCANHandle->counters.bit_timing = btr;

    /* Filter 0: 32-bit mask=0, nhan tat ca standard/ext/data/RTR vao FIFO0. */
    pCANHandle->pCANx->FMR = 1U;
    pCANHandle->pCANx->FA1R = 0U;
    pCANHandle->pCANx->FM1R = 0U;
    pCANHandle->pCANx->FS1R = 1U;
    pCANHandle->pCANx->FFA1R = 0U;
    pCANHandle->pCANx->FILTER[0].ID = 0U;
    pCANHandle->pCANx->FILTER[0].MASK = 0U;
    pCANHandle->pCANx->FA1R = 1U;
    pCANHandle->pCANx->FMR = 0U;
    pCANHandle->pCANx->TSR = CAN_ALL_COMPLETED;
    pCANHandle->pCANx->MCR &= ~CAN_MCR_INRQ;
    if (!wait_init_ack(pCANHandle, false)) {
        return init_failed(pCANHandle);
    }

    key = IRQ_Save();
    /* Khong bat LECIE tung loi de tranh ISR storm khi bus chua co ACK. */
    pCANHandle->pCANx->IER = (1UL << 0) | (1UL << 1) | (1UL << 3) |
                            (1UL << 4) | (1UL << 6) | (1UL << 8) |
                            (1UL << 9) | (1UL << 10) | (1UL << 15);
    pCANHandle->initialized = true;
    for (uint8_t irq = CAN_IRQ_TX; irq <= CAN_IRQ_SCE; ++irq) {
        NVIC_SetPriority(irq, pCANHandle->CAN_Config.CAN_IRQPriority);
        NVIC_ClearPendingIRQ(irq);
        NVIC_EnableIRQ(irq);
    }
    IRQ_Restore(key);
    return true;
}

bool CAN_DeInit(CAN_Handle_t *pCANHandle)
{
    uint32_t key = IRQ_Save();
    if (!valid_handle(pCANHandle)) {
        IRQ_Restore(key);
        return false;
    }
    collect_completions(pCANHandle);
    if (pCANHandle->tx_count != 0U) {
        IRQ_Restore(key);
        return false;
    }
    for (uint8_t i = 0; i < 3U; ++i) {
        if (pCANHandle->inflight[i].active) {
            IRQ_Restore(key);
            return false;
        }
    }
    pCANHandle->pCANx->IER = 0U;
    for (uint8_t irq = CAN_IRQ_TX; irq <= CAN_IRQ_SCE; ++irq) {
        NVIC_DisableIRQ(irq);
        NVIC_ClearPendingIRQ(irq);
    }
    CAN1_REG_RESET();
    CAN_PeriClockControl(pCANHandle->pCANx, DISABLE);
    pCANHandle->rx_head = pCANHandle->rx_tail = pCANHandle->rx_count = 0U;
    pCANHandle->result_head = pCANHandle->result_tail = pCANHandle->result_count = 0U;
    pCANHandle->initialized = false;
    bound_handle = NULL;
    IRQ_Restore(key);
    return true;
}

bool CAN_Send(CAN_Handle_t *pCANHandle, const CAN_Frame *frame)
{
    if (!valid_frame(frame)) {
        return false;
    }
    CAN_Frame copy = clean_frame(frame);
    uint32_t key = IRQ_Save();
    if (!valid_handle(pCANHandle)) {
        IRQ_Restore(key);
        return false;
    }
    collect_completions(pCANHandle);
    sample_bus_state(pCANHandle);
    if (pCANHandle->tx_count == CAN_TX_QUEUE_SIZE) {
        ++pCANHandle->counters.tx_queue_full;
        IRQ_Restore(key);
        return false;
    }
    pCANHandle->tx_queue[pCANHandle->tx_head] = copy;
    pCANHandle->tx_head = (uint8_t)((pCANHandle->tx_head + 1U) % CAN_TX_QUEUE_SIZE);
    ++pCANHandle->tx_count;
    ++pCANHandle->counters.tx_queued;
    schedule_tx(pCANHandle);
    IRQ_Restore(key);
    return true;
}

bool CAN_Receive(CAN_Handle_t *pCANHandle, CAN_Frame *frame)
{
    if (frame == NULL) {
        return false;
    }
    uint32_t key = IRQ_Save();
    if (!valid_handle(pCANHandle) || pCANHandle->rx_count == 0U) {
        IRQ_Restore(key);
        return false;
    }
    *frame = pCANHandle->rx_queue[pCANHandle->rx_tail];
    pCANHandle->rx_tail = (uint8_t)((pCANHandle->rx_tail + 1U) % CAN_RX_QUEUE_SIZE);
    --pCANHandle->rx_count;
    IRQ_Restore(key);
    return true;
}

bool CAN_GetTxResult(CAN_Handle_t *pCANHandle, CAN_TxResult *result)
{
    if (result == NULL) {
        return false;
    }
    uint32_t key = IRQ_Save();
    if (!valid_handle(pCANHandle) || pCANHandle->result_count == 0U) {
        IRQ_Restore(key);
        return false;
    }
    *result = pCANHandle->result_queue[pCANHandle->result_tail];
    pCANHandle->result_tail = (uint8_t)((pCANHandle->result_tail + 1U) % CAN_RESULT_QUEUE_SIZE);
    --pCANHandle->result_count;
    IRQ_Restore(key);
    return true;
}

void CAN_GetStats(CAN_Handle_t *pCANHandle, CAN_Stats *stats)
{
    if (stats == NULL) {
        return;
    }
    uint32_t key = IRQ_Save();
    if (pCANHandle != NULL) {
        *stats = pCANHandle->counters;
    } else {
        *stats = (CAN_Stats){0};
    }
    IRQ_Restore(key);
}

void CAN_Process(CAN_Handle_t *pCANHandle)
{
    uint32_t key = IRQ_Save();
    if (valid_handle(pCANHandle)) {
        collect_completions(pCANHandle);
        sample_bus_state(pCANHandle);
        uint32_t now = Timebase_Millis();
        for (uint8_t i = 0; i < 3U; ++i) {
            if (pCANHandle->inflight[i].active && !pCANHandle->inflight[i].abort_requested &&
                (uint32_t)(now - pCANHandle->inflight[i].started_ms) >= pCANHandle->CAN_Config.CAN_TxTimeoutMs) {
                pCANHandle->inflight[i].abort_requested = true;
                pCANHandle->inflight[i].abort_reason = CAN_TX_TIMEOUT;
                ++pCANHandle->counters.tx_timeouts;
                pCANHandle->pCANx->TSR = 1UL << (7U + (uint32_t)i * 8U);
            }
        }
        schedule_tx(pCANHandle);
    }
    IRQ_Restore(key);
}

void CAN_TXIRQHandling(CAN_Handle_t *pCANHandle)
{
    uint32_t key = IRQ_Save();
    if (valid_handle(pCANHandle)) {
        collect_completions(pCANHandle);
        sample_bus_state(pCANHandle);
        schedule_tx(pCANHandle);
    }
    IRQ_Restore(key);
}

void CAN_RX0IRQHandling(CAN_Handle_t *pCANHandle)
{
    uint32_t key = IRQ_Save();
    if (valid_handle(pCANHandle)) {
        drain_rx(pCANHandle, 0U);
    }
    IRQ_Restore(key);
}

void CAN_RX1IRQHandling(CAN_Handle_t *pCANHandle)
{
    uint32_t key = IRQ_Save();
    if (valid_handle(pCANHandle)) {
        drain_rx(pCANHandle, 1U);
    }
    IRQ_Restore(key);
}

void CAN_SCEIRQHandling(CAN_Handle_t *pCANHandle)
{
    uint32_t key = IRQ_Save();
    if (valid_handle(pCANHandle)) {
        ++pCANHandle->counters.error_interrupts;
        sample_bus_state(pCANHandle);
        /* MSR ERRI/WKUI/SLAKI la W1C; khong RMW. LEC duoc giu de chan doan. */
        pCANHandle->pCANx->MSR = (1UL << 2) | (1UL << 3) | (1UL << 4);
    }
    IRQ_Restore(key);
}
