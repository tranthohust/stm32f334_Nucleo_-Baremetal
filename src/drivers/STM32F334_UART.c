#include "STM32F334_UART.h"
#include "STM32F334_CORE.h"
#include "STM32F334.h"

/* Doi chieu ST RM0364 chuong DMA/USART va CMSIS stm32f334x8.h.
 * Day la driver cu the USART2 de de doc; khong co HAL, heap hay callback ISR. */

typedef struct {
    volatile uint32_t CCR;
    volatile uint32_t CNDTR;
    volatile uint32_t CPAR;
    volatile uint32_t CMAR;
} UART_DMA_Channel;

#define UART_DMA_ISR   (*(volatile uint32_t *)0x40020000U)
#define UART_DMA_IFCR  (*(volatile uint32_t *)0x40020004U)
#define UART_DMA_RX    ((UART_DMA_Channel *)0x4002006CU)
#define UART_DMA_TX    ((UART_DMA_Channel *)0x40020080U)

#define DMA_EN        (1U << 0)
#define DMA_TCIE      (1U << 1)
#define DMA_HTIE      (1U << 2)
#define DMA_TEIE      (1U << 3)
#define DMA_DIR       (1U << 4)
#define DMA_CIRC      (1U << 5)
#define DMA_MINC      (1U << 7)
#define DMA_HIGH      (2U << 12)
#define RX_DMA_FLAGS  (15U << 20)
#define RX_DMA_TC     (2U << 20)
#define RX_DMA_HT     (4U << 20)
#define RX_DMA_TE     (8U << 20)
#define TX_DMA_FLAGS  (15U << 24)
#define TX_DMA_TC     (2U << 24)
#define TX_DMA_TE     (8U << 24)

#define UART_UE       (1U << 0)
#define UART_RE       (1U << 2)
#define UART_TE       (1U << 3)
#define UART_IDLEIE   (1U << 4)
#define UART_RXNEIE   (1U << 5)
#define UART_PEIE     (1U << 8)
#define UART_EIE      (1U << 0)
#define UART_DMAR     (1U << 6)
#define UART_DMAT     (1U << 7)
#define UART_ERRORS   15U
#define UART_IDLE     (1U << 4)
#define UART_RXNE     (1U << 5)
#define UART_TC       (1U << 6)
#define UART_RXFRQ    (1U << 3)
#define UART_ACKS     ((1U << 21) | (1U << 22))

#define RX_SIZE       UART_RX_BUFFER_SIZE
#define TX_SIZE       UART_TX_BUFFER_SIZE
#define RAW_SIZE      UART_DMA_BUFFER_SIZE
#define USART2_IRQ    38U
#define DMA_RX_IRQ    16U
#define DMA_TX_IRQ    17U

_Static_assert(offsetof(USART_RegDef_t, RDR) == 0x24U, "USART RDR offset");
_Static_assert(offsetof(USART_RegDef_t, TDR) == 0x28U, "USART TDR offset");
_Static_assert(offsetof(UART_DMA_Channel, CMAR) == 0x0CU, "DMA CMAR offset");

/* Chi con binding phan cung dung chung; buffer/trang thai deu o trong handle.
 * Khong doc owner cu trong Init fail: caller duoc phep huy handle fail ngay. */
static UART_Handle_t *volatile uart2_owner;

static bool uart_owned(const UART_Handle_t *handle)
{
    return handle != NULL && handle == uart2_owner && handle->Initialized &&
           handle->pUSARTx == USART2;
}

/* Chi goi khi IRQ da khoa. Head duoc publish SAU khi byte da ghi xong. */
static void rx_push(UART_Handle_t *handle, uint8_t value)
{
    uint16_t next = (uint16_t)((handle->RxHead + 1U) % RX_SIZE);
    ++handle->Stats.rx_bytes;
    if (next == handle->RxTail) {
        /* Drop newest: tuyet doi khong ghi de byte main chua doc. */
        ++handle->Stats.rx_dropped;
        ++handle->Stats.rx_epoch;
        return;
    }
    handle->RxBuffer[handle->RxHead] = value;
    handle->RxHead = next;
}

/* DMAR=0 truoc khi sua channel. IFCR la write-one-to-clear: ghi mask truc tiep. */
static void rx_dma_restart(UART_Handle_t *handle)
{
    handle->pUSARTx->CR3 &= ~UART_DMAR;
    UART_DMA_RX->CCR &= ~DMA_EN;
    UART_DMA_IFCR = RX_DMA_FLAGS;
    UART_DMA_RX->CPAR = (uint32_t)(uintptr_t)&handle->pUSARTx->RDR;
    UART_DMA_RX->CMAR = (uint32_t)(uintptr_t)handle->RxDMABuffer;
    UART_DMA_RX->CNDTR = RAW_SIZE;
    handle->DMAWrapBytes = 0U;
    handle->DMAReadTotal = 0U;
    UART_DMA_RX->CCR = DMA_MINC | DMA_CIRC | DMA_HIGH |
                       DMA_TCIE | DMA_HTIE | DMA_TEIE | DMA_EN;
    handle->pUSARTx->CR3 |= UART_DMAR;
}

/* TC duoc tinh thanh tong tien trinh de phan biet pos==old_pos sau mot vong.
 * Doc co DMA flag truoc/ sau CNDTR de tranh snapshot dung luc DMA wrap.
 * IRQ_Save khoa CPU ISR, KHONG dung DMA. Phai ton trong latency trong header. */
static void rx_dma_collect(UART_Handle_t *handle)
{
    uint32_t before, after, remaining;
    do {
        before = UART_DMA_ISR & RX_DMA_FLAGS;
        remaining = UART_DMA_RX->CNDTR;
        after = UART_DMA_ISR & RX_DMA_FLAGS;
    } while (((before ^ after) & RX_DMA_TC) != 0U);

    if ((after & RX_DMA_TE) != 0U) {
        ++handle->Stats.dma_errors;
        ++handle->Stats.rx_errors;
        ++handle->Stats.rx_epoch;
        handle->RxTail = handle->RxHead; /* Du lieu truoc/sau loi khong duoc ghep thanh mot goi. */
        rx_dma_restart(handle);
        return;
    }

    /* CNDTR=0 la ranh gioi reload. Neu TC chua toi (hoac da duoc lay),
     * cho snapshot tiep theo thay vi doan them mot vong tu vi tri zero. */
    if (remaining == 0U && (after & RX_DMA_TC) == 0U)
        return;

    /* Xoa dung cac co da snapshot; khong xoa TC moi toi sau snapshot. */
    UART_DMA_IFCR = after & (RX_DMA_TC | RX_DMA_HT);
    if ((after & RX_DMA_TC) != 0U)
        handle->DMAWrapBytes += RAW_SIZE;

    uint32_t position = RAW_SIZE - remaining;
    if ((after & RX_DMA_TC) != 0U && position == RAW_SIZE)
        position = 0U; /* TC da tinh vong cu; CNDTR co the vua o 0 truoc reload. */
    uint32_t producer = handle->DMAWrapBytes + position;
    uint32_t available = producer - handle->DMAReadTotal;

    if (available > RAW_SIZE) {
        /* Da bi ghi de: bo vung khong con tin cay, bao parser resync. */
        ++handle->Stats.rx_dma_overruns;
        ++handle->Stats.rx_errors;
        ++handle->Stats.rx_epoch;
        handle->Stats.rx_dropped += available;
        handle->RxTail = handle->RxHead;
        handle->DMAReadTotal = producer;
        return;
    }
    while (available != 0U) {
        rx_push(handle, handle->RxDMABuffer[handle->DMAReadTotal % RAW_SIZE]);
        ++handle->DMAReadTotal;
        --available;
    }
}

static void tx_finish(UART_Handle_t *handle, uint32_t flags)
{
    if (!handle->TxBusy || (flags & (TX_DMA_TC | TX_DMA_TE)) == 0U)
        return;

    handle->pUSARTx->CR3 &= ~UART_DMAT;
    UART_DMA_TX->CCR &= ~DMA_EN;
    uint32_t remaining = UART_DMA_TX->CNDTR;
    UART_DMA_IFCR = TX_DMA_FLAGS;
    if ((flags & TX_DMA_TE) != 0U) {
        ++handle->Stats.dma_errors;
        /* Khong replay goi gui do: bo phan chua gui, caller thay counter loi. */
        if (remaining > handle->TxActiveLength)
            remaining = handle->TxActiveLength;
        handle->Stats.tx_bytes += handle->TxActiveLength - remaining;
        handle->Stats.tx_dropped += remaining;
    } else {
        handle->Stats.tx_bytes += handle->TxActiveLength;
    }
    handle->TxTail = (uint16_t)((handle->TxTail + handle->TxActiveLength) % TX_SIZE);
    handle->TxActiveLength = 0U;
    handle->TxBusy = false;
}

void UART_PeriClockControl(USART_RegDef_t *pUSARTx, uint8_t EnorDi)
{
    if (pUSARTx != USART2)
        return;
    if (EnorDi == ENABLE)
        USART2_PCLK_EN();
    else
        USART2_PCLK_DI();
}

bool UART_Init(UART_Handle_t *handle)
{
    if (handle == NULL)
        return false;
    uint32_t key = IRQ_Save();
    /* Khong ghi de owner/queue dang hoat dong, ke ca caller goi lai Init. */
    if (uart2_owner != NULL || handle->pUSARTx != USART2) {
        IRQ_Restore(key);
        return false;
    }
    uint32_t pclk_hz = handle->UART_Config.UART_PCLKHz;
    uint32_t baud = handle->UART_Config.UART_BaudRate;
    UART_RxMode mode = handle->UART_Config.UART_RxMode;
    uint8_t priority = handle->UART_Config.UART_IRQPriority;
    if (pclk_hz == 0U || pclk_hz > 72000000U || baud == 0U ||
        baud > pclk_hz / 16U || priority > 15U ||
        (mode != UART_RX_IRQ && mode != UART_RX_DMA)) {
        IRQ_Restore(key);
        return false;
    }
    uint32_t divisor = (pclk_hz + baud / 2U) / baud;
    if (divisor < 16U || divisor > 65535U) {
        IRQ_Restore(key);
        return false;
    }

    handle->Initialized = false;
    NVIC_DisableIRQ(USART2_IRQ);
    NVIC_DisableIRQ(DMA_RX_IRQ);
    NVIC_DisableIRQ(DMA_TX_IRQ);
    RCC->AHBENR |= 1U;                 /* DMA1 clock, khong reset DMA cua SPI. */
    UART_PeriClockControl(handle->pUSARTx, ENABLE);
    UART_DMA_RX->CCR = 0U;
    UART_DMA_TX->CCR = 0U;
    UART_DMA_IFCR = RX_DMA_FLAGS | TX_DMA_FLAGS;
    RCC->APB1RSTR |= 1U << 17;
    RCC->APB1RSTR &= ~(1U << 17);

    handle->RxHead = handle->RxTail = handle->TxHead = handle->TxTail = 0U;
    handle->TxBusy = false;
    handle->TxActiveLength = 0U;
    handle->Stats.rx_bytes = handle->Stats.tx_bytes = handle->Stats.rx_dropped = 0U;
    handle->Stats.rx_errors = handle->Stats.dma_errors = handle->Stats.rx_epoch = 0U;
    handle->Stats.tx_dropped = handle->Stats.rx_dma_overruns = 0U;
    handle->DMAWrapBytes = handle->DMAReadTotal = 0U;
    handle->ActiveRxMode = mode;
    /* USART2 dung PCLK1, OVER8=0: BRR = round(PCLK1/baud). */
    handle->pUSARTx->BRR = divisor;
    handle->pUSARTx->CR2 = 0U;
    handle->pUSARTx->CR3 = UART_EIE;
    handle->pUSARTx->ICR = UART_ERRORS | UART_IDLE | UART_TC;
    handle->pUSARTx->CR1 = UART_RE | UART_TE | UART_PEIE |
                    (mode == UART_RX_DMA ? UART_IDLEIE : UART_RXNEIE);
    if (mode == UART_RX_DMA)
        rx_dma_restart(handle);
    handle->pUSARTx->CR1 |= UART_UE;

    /* Chi init doi ngan; duong du lieu Process khong cho blocking. */
    uint32_t attempts = 100000U;
    while ((handle->pUSARTx->ISR & UART_ACKS) != UART_ACKS && attempts != 0U)
        --attempts;
    if (attempts == 0U) {
        handle->pUSARTx->CR3 = 0U;
        handle->pUSARTx->CR1 = 0U;
        UART_DMA_RX->CCR = 0U;
        UART_DMA_TX->CCR = 0U;
        UART_DMA_IFCR = RX_DMA_FLAGS | TX_DMA_FLAGS;
        NVIC_ClearPendingIRQ(USART2_IRQ);
        NVIC_ClearPendingIRQ(DMA_RX_IRQ);
        NVIC_ClearPendingIRQ(DMA_TX_IRQ);
        UART_PeriClockControl(handle->pUSARTx, DISABLE);
        IRQ_Restore(key);
        return false;
    }

    NVIC_ClearPendingIRQ(USART2_IRQ);
    NVIC_ClearPendingIRQ(DMA_RX_IRQ);
    NVIC_ClearPendingIRQ(DMA_TX_IRQ);
    NVIC_SetPriority(USART2_IRQ, priority);
    NVIC_SetPriority(DMA_RX_IRQ, priority);
    NVIC_SetPriority(DMA_TX_IRQ, priority);
    handle->Initialized = true;
    uart2_owner = handle; /* Chi bind khi phan cung da san sang. */
    NVIC_EnableIRQ(USART2_IRQ);
    if (mode == UART_RX_DMA)
        NVIC_EnableIRQ(DMA_RX_IRQ);
    NVIC_EnableIRQ(DMA_TX_IRQ);
    IRQ_Restore(key);
    return true;
}

bool UART_DeInit(UART_Handle_t *handle)
{
    uint32_t key = IRQ_Save();
    if (handle == NULL || handle != uart2_owner) {
        IRQ_Restore(key);
        return false;
    }
    NVIC_DisableIRQ(USART2_IRQ);
    NVIC_DisableIRQ(DMA_RX_IRQ);
    NVIC_DisableIRQ(DMA_TX_IRQ);
    /* Binding chi danh cho USART2: quiesce dung ngoai vi da bind, ngay ca
     * caller vo tinh sua pUSARTx. Khong reset DMA clock cua ngoai vi khac. */
    USART2->CR3 = 0U;
    USART2->CR1 = 0U;
    UART_DMA_RX->CCR = 0U;
    UART_DMA_TX->CCR = 0U;
    UART_DMA_IFCR = RX_DMA_FLAGS | TX_DMA_FLAGS;
    NVIC_ClearPendingIRQ(USART2_IRQ);
    NVIC_ClearPendingIRQ(DMA_RX_IRQ);
    NVIC_ClearPendingIRQ(DMA_TX_IRQ);
    RCC->APB1RSTR |= 1U << 17;
    RCC->APB1RSTR &= ~(1U << 17);
    UART_PeriClockControl(USART2, DISABLE);
    /* Abort la hanh dong tuong minh. Bao so byte RAM dang giu bi huy;
     * tx_dropped o day tinh bao thu ca span DMA chua co TC. */
    handle->Stats.rx_dropped += (handle->RxHead + RX_SIZE - handle->RxTail) % RX_SIZE;
    handle->Stats.tx_dropped += (handle->TxHead + TX_SIZE - handle->TxTail) % TX_SIZE;
    ++handle->Stats.rx_epoch;
    handle->RxHead = handle->RxTail = handle->TxHead = handle->TxTail = 0U;
    handle->TxActiveLength = 0U;
    handle->TxBusy = false;
    handle->Initialized = false;
    uart2_owner = NULL;
    IRQ_Restore(key);
    return true;
}

size_t UART_Write(UART_Handle_t *handle, const uint8_t *data, size_t length)
{
    if (!uart_owned(handle) || data == NULL)
        return 0U;
    uint32_t key = IRQ_Save();
    size_t accepted = 0U;
    while (accepted < length) {
        uint16_t next = (uint16_t)((handle->TxHead + 1U) % TX_SIZE);
        if (next == handle->TxTail)
            break;
        handle->TxBuffer[handle->TxHead] = data[accepted++];
        handle->TxHead = next;
    }
    handle->Stats.tx_dropped += (uint32_t)(length - accepted);
    IRQ_Restore(key);
    UART_Process(handle);
    return accepted;
}

size_t UART_Read(UART_Handle_t *handle, uint8_t *data, size_t capacity)
{
    if (!uart_owned(handle) || data == NULL)
        return 0U;
    uint32_t key = IRQ_Save();
    size_t received = 0U;
    while (received < capacity && handle->RxTail != handle->RxHead) {
        data[received++] = handle->RxBuffer[handle->RxTail];
        handle->RxTail = (uint16_t)((handle->RxTail + 1U) % RX_SIZE);
    }
    IRQ_Restore(key);
    return received;
}

void UART_Process(UART_Handle_t *handle)
{
    if (!uart_owned(handle))
        return;
    uint32_t key = IRQ_Save();
    tx_finish(handle, UART_DMA_ISR & TX_DMA_FLAGS);
    if (!handle->TxBusy && handle->TxHead != handle->TxTail) {
        uint16_t count = handle->TxHead > handle->TxTail ? (uint16_t)(handle->TxHead - handle->TxTail) :
                                            (uint16_t)(TX_SIZE - handle->TxTail);
        /* DMA chi doc mot doan lien tuc. Sau TC moi giai phong doan nay. */
        UART_DMA_TX->CCR = 0U;
        UART_DMA_IFCR = TX_DMA_FLAGS;
        UART_DMA_TX->CPAR = (uint32_t)(uintptr_t)&handle->pUSARTx->TDR;
        UART_DMA_TX->CMAR = (uint32_t)(uintptr_t)&handle->TxBuffer[handle->TxTail];
        UART_DMA_TX->CNDTR = count;
        handle->TxActiveLength = count;
        handle->TxBusy = true;
        handle->pUSARTx->ICR = UART_TC;
        UART_DMA_TX->CCR = DMA_DIR | DMA_MINC | DMA_TCIE | DMA_TEIE | DMA_EN;
        handle->pUSARTx->CR3 |= UART_DMAT;
    }
    IRQ_Restore(key);
}

bool UART_TxIdle(UART_Handle_t *handle)
{
    uint32_t key = IRQ_Save();
    bool idle = uart_owned(handle) && !handle->TxBusy && handle->TxHead == handle->TxTail &&
                (handle->pUSARTx->ISR & UART_TC) != 0U;
    IRQ_Restore(key);
    return idle;
}

void UART_GetStats(UART_Handle_t *handle, UART_Stats *out)
{
    if (!uart_owned(handle) || out == NULL)
        return;
    uint32_t key = IRQ_Save();
    out->rx_bytes = handle->Stats.rx_bytes;
    out->tx_bytes = handle->Stats.tx_bytes;
    out->rx_dropped = handle->Stats.rx_dropped;
    out->rx_errors = handle->Stats.rx_errors;
    out->dma_errors = handle->Stats.dma_errors;
    out->rx_epoch = handle->Stats.rx_epoch;
    out->tx_dropped = handle->Stats.tx_dropped;
    out->rx_dma_overruns = handle->Stats.rx_dma_overruns;
    IRQ_Restore(key);
}

uint32_t UART_RxEpoch(UART_Handle_t *handle)
{
    return uart_owned(handle) ? handle->Stats.rx_epoch : 0U;
}

void UART_DiscardRx(UART_Handle_t *handle)
{
    if (!uart_owned(handle))
        return;
    uint32_t key = IRQ_Save();
    handle->RxTail = handle->RxHead;
    IRQ_Restore(key);
}

void UART_IRQHandling(UART_Handle_t *handle)
{
    if (!uart_owned(handle))
        return;
    uint32_t key = IRQ_Save();
    uint32_t status = handle->pUSARTx->ISR;
    if ((status & UART_ERRORS) != 0U) {
        handle->pUSARTx->ICR = status & UART_ERRORS;
        ++handle->Stats.rx_errors;
        ++handle->Stats.rx_epoch;
        handle->RxTail = handle->RxHead;
        if (handle->ActiveRxMode == UART_RX_DMA) {
            handle->pUSARTx->CR3 &= ~UART_DMAR;
            UART_DMA_RX->CCR &= ~DMA_EN;
        }
        /* F334 xoa IDLE/loi bang ICR; khong dung meo doc SR/DR cua STM32F1. */
        handle->pUSARTx->RQR = UART_RXFRQ;
        if (handle->ActiveRxMode == UART_RX_DMA)
            rx_dma_restart(handle);
    } else if (handle->ActiveRxMode == UART_RX_IRQ && (status & UART_RXNE) != 0U) {
        rx_push(handle, (uint8_t)handle->pUSARTx->RDR);
    }
    if ((status & UART_IDLE) != 0U) {
        handle->pUSARTx->ICR = UART_IDLE;
        if (handle->ActiveRxMode == UART_RX_DMA)
            rx_dma_collect(handle);
    }
    IRQ_Restore(key);
}

void UART_DMARxIRQHandling(UART_Handle_t *handle)
{
    if (!uart_owned(handle) || handle->ActiveRxMode != UART_RX_DMA)
        return;
    uint32_t key = IRQ_Save();
    rx_dma_collect(handle);
    IRQ_Restore(key);
}

void UART_DMATxIRQHandling(UART_Handle_t *handle)
{
    if (!uart_owned(handle))
        return;
    uint32_t key = IRQ_Save();
    tx_finish(handle, UART_DMA_ISR & TX_DMA_FLAGS);
    IRQ_Restore(key);
}
