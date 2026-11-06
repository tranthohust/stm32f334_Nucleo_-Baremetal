#include "STM32F334_ADC.h"
#include "STM32F334_CORE.h"
#include <stddef.h>

#define ADC_IRQ 18U
#define ADC_EN 1U
#define ADC_START (1U << 2)
#define ADC_CAL (1UL << 31)
#define ADC_VREG_MASK (3UL << 28)
#define ADC_VREG_ON (1UL << 28)
#define ADC_CONTROL_BUSY (0x3FU | ADC_CAL)
#define ADC_READY 1U
#define ADC_EOC (1U << 2)
#define ADC_EOS (1U << 3)
#define ADC_OVR (1U << 4)
#define ADC_FLAGS (ADC_READY | ADC_EOC | ADC_EOS | ADC_OVR)
#define ADC_CLOCK (1UL << 28)
#define ADC_INIT_TIMEOUT 20U
#define ADC_MAX_POLLS 1000000U
_Static_assert(offsetof(ADC_RegDef_t, SMPR1) == 0x14U, "ADC SMPR1 offset");
_Static_assert(offsetof(ADC_RegDef_t, SQR1) == 0x30U, "ADC SQR1 offset");
_Static_assert(offsetof(ADC_RegDef_t, DR) == 0x40U, "ADC DR offset");
_Static_assert(offsetof(ADC_RegDef_t, DIFSEL) == 0xB0U, "ADC DIFSEL offset");
_Static_assert(offsetof(ADC_Common_RegDef_t, CCR) == 8U, "ADC common CCR offset");
static ADC_Handle_t *owner;

static bool owned(const ADC_Handle_t *h)
{
    return h != NULL && h == owner && h->pADCx == ADC1 && h->Initialized;
}

void ADC_PeriClockControl(ADC_RegDef_t *adc, uint8_t enable)
{
    if (adc != ADC1) return;
    uint32_t key = IRQ_Save();
    if (enable == ENABLE) RCC->AHBENR |= ADC_CLOCK;
    else if (enable == DISABLE) RCC->AHBENR &= ~ADC_CLOCK;
    (void)RCC->AHBENR;
    IRQ_Restore(key);
}

static void reset_group(void)
{
    RCC->AHBRSTR |= ADC_CLOCK;
    RCC->AHBRSTR &= ~ADC_CLOCK;
}

static bool wait_mask(volatile uint32_t *reg, uint32_t mask, bool set)
{
    uint32_t start = Timebase_Millis();
    for (uint32_t n = 0U; n < ADC_MAX_POLLS; ++n) {
        if (((*reg & mask) != 0U) == set) return true;
        if ((uint32_t)(Timebase_Millis() - start) >= ADC_INIT_TIMEOUT) break;
    }
    return false;
}

static bool settle(void)
{
    /* Doi >=1ms thay cho busy delay phu thuoc compiler. Hai lan doi tach biet:
     * VREG can >=10us; sau calibration can >=4 ADC clock (HCLK/2 = 4MHz).
     * Doi QUA bien tick tiep theo chua chac du1ms, nen doi 2 tick cho margin. */
    uint32_t start = Timebase_Millis();
    for (uint32_t n = 0U; n < ADC_MAX_POLLS; ++n)
        if ((uint32_t)(Timebase_Millis() - start) >= 2U) return true;
    return false;
}

static void shutdown(ADC_Handle_t *h)
{
    ADC1->IER = 0U;
    NVIC_DisableIRQ(ADC_IRQ);
    NVIC_ClearPendingIRQ(ADC_IRQ);
    /* Group reset chi duoc dung vi driver doc quyen ADC12 theo contract. */
    reset_group();
    ADC_PeriClockControl(ADC1, DISABLE);
    h->Initialized = false;
    h->Busy = false;
    owner = NULL;
}

bool ADC_Init(ADC_Handle_t *h)
{
    if (h == NULL || h->pADCx != ADC1 || h->ADC_Config.ADC_CoreClockHz != 8000000U ||
        h->ADC_Config.ADC_Channel < 1U || h->ADC_Config.ADC_Channel > 15U ||
        h->ADC_Config.ADC_SampleTime > 7U || h->ADC_Config.ADC_IRQPriority > 15U)
        return false;
    uint32_t key = IRQ_Save();
    if (owner != NULL || h->ProcessRunning || key != 0U || (RCC->CFGR & 0xFCU) != 0U) {
        IRQ_Restore(key); return false;
    }
    ADC_PeriClockControl(ADC1, ENABLE);
    if ((ADC2->CR & ADC_CONTROL_BUSY) != 0U || (ADC1->CR & ADC_CONTROL_BUSY) != 0U ||
        (ADC12_COMMON->CCR & 0x1FU) != 0U) {
        IRQ_Restore(key); return false;
    }
    owner = h; /* Reserve ngay, nhung IRQ chi hop le sau Initialized. */
    h->Initialized = h->Busy = h->Done = h->ProcessRunning = false;
    h->Samples = h->Errors = 0U;
    h->Value = 0U;
    h->Callback = NULL;
    h->Context = NULL;
    NVIC_DisableIRQ(ADC_IRQ);
    reset_group();
    ADC12_COMMON->CCR = 2UL << 16; /* Synchronous HCLK/2, independent mode. */
    ADC1->IER = 0U;
    ADC1->CFGR = 0U; /* 12-bit, right aligned, no continuous, no external trigger. */
    ADC1->DIFSEL = 0U;
    uint8_t channel = h->ADC_Config.ADC_Channel;
    ADC1->SMPR1 = channel < 10U ? (uint32_t)h->ADC_Config.ADC_SampleTime << (3U * channel) : 0U;
    ADC1->SMPR2 = channel >= 10U ? (uint32_t)h->ADC_Config.ADC_SampleTime << (3U * (channel - 10U)) : 0U;
    ADC1->SQR1 = (uint32_t)channel << 6; /* L=0: 1 conversion; SQ1=channel. */
    ADC1->SQR2 = ADC1->SQR3 = ADC1->SQR4 = 0U;
    ADC1->CR = (ADC1->CR & ~ADC_VREG_MASK); /* Reset ADVREGEN 10 -> 00 -> 01. */
    ADC1->CR |= ADC_VREG_ON;
    IRQ_Restore(key); /* Khong doi calibration voi PRIMASK=1. */
    if (!settle()) goto fail;
    ADC1->CR |= ADC_CAL; /* Single-ended calibration, ADCALDIF=0 after reset. */
    if (!wait_mask(&ADC1->CR, ADC_CAL, false) || !settle()) goto fail;
    ADC1->ISR = ADC_FLAGS; /* W1C, khong |=. */
    ADC1->CR |= ADC_EN;
    if (!wait_mask(&ADC1->ISR, ADC_READY, true)) goto fail;
    key = IRQ_Save();
    NVIC_SetPriority(ADC_IRQ, h->ADC_Config.ADC_IRQPriority);
    NVIC_ClearPendingIRQ(ADC_IRQ);
    h->Initialized = true;
    NVIC_EnableIRQ(ADC_IRQ);
    IRQ_Restore(key);
    return true;
fail:
    key = IRQ_Save();
    shutdown(h);
    IRQ_Restore(key);
    return false;
}

bool ADC_DeInit(ADC_Handle_t *h)
{
    uint32_t key = IRQ_Save();
    if (!owned(h) || h->Busy || h->ProcessRunning || (ADC2->CR & ADC_CONTROL_BUSY) != 0U) {
        IRQ_Restore(key); return false;
    }
    h->Done = false;
    shutdown(h);
    IRQ_Restore(key);
    return true;
}

bool ADC_Start_IT(ADC_Handle_t *h, uint32_t timeout_ms, ADC_Callback callback, void *context)
{
    uint32_t key = IRQ_Save();
    if (!owned(h) || h->Busy || h->Done || timeout_ms == 0U || timeout_ms > 0x7FFFFFFFU ||
        (h->pADCx->CR & (ADC_START | ADC_CAL)) != 0U || (h->pADCx->CR & ADC_EN) == 0U) {
        IRQ_Restore(key); return false;
    }
    h->Callback = callback;
    h->Context = context;
    h->Value = 0U;
    h->Result = ADC_OK;
    h->StartedMs = Timebase_Millis();
    h->TimeoutMs = timeout_ms;
    h->Busy = true;
    h->pADCx->ISR = ADC_EOC | ADC_EOS | ADC_OVR;
    h->pADCx->IER = ADC_EOC | ADC_OVR;
    h->pADCx->CR |= ADC_START;
    IRQ_Restore(key);
    return true;
}

void ADC_IRQHandling(ADC_Handle_t *h)
{
    uint32_t key = IRQ_Save();
    if (owned(h) && h->Busy) {
        uint32_t status = h->pADCx->ISR;
        if ((status & (ADC_EOC | ADC_OVR)) != 0U) {
            if ((status & ADC_OVR) != 0U) {
                (void)h->pADCx->DR;
                h->Result = ADC_OVERRUN;
                ++h->Errors;
            }
            else { h->Value = (uint16_t)(h->pADCx->DR & 4095U); ++h->Samples; }
            h->pADCx->ISR = status & (ADC_EOC | ADC_EOS | ADC_OVR);
            h->pADCx->IER = 0U;
            h->Busy = false;
            h->Done = true;
        }
    }
    IRQ_Restore(key);
}

void ADC_Process(ADC_Handle_t *h)
{
    uint32_t key = IRQ_Save();
    if (!owned(h) || h->ProcessRunning) { IRQ_Restore(key); return; }
    h->ProcessRunning = true;
    ADC_IRQHandling(h); /* Poll flags da den truoc khi xet deadline. */
    if (h->Busy && (uint32_t)(Timebase_Millis() - h->StartedMs) >= h->TimeoutMs) {
        h->Result = ADC_TIMEOUT;
        ++h->Errors;
        h->Done = true;
        shutdown(h); /* Khong cho sample cua lan timeout lot vao lan Start sau. */
    }
    bool done = h->Done;
    ADC_Callback callback = h->Callback;
    ADC_Result result = h->Result;
    uint16_t value = h->Value;
    void *context = h->Context;
    h->Done = false;
    IRQ_Restore(key);
    if (done && callback != NULL) callback(result, value, context);
    h->ProcessRunning = false;
}
