#include "../src/drivers/STM32F334_ADC.h"
#include "../src/drivers/STM32F334_CORE.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

static ADC_RegDef_t adc1, adc2;
static ADC_Common_RegDef_t common;
static RCC_RegDef_t rcc;
static uint32_t mask, now;
static bool calibrates = true, ready = true, frozen;
static bool irq_enabled;
#undef ADC1
#undef ADC2
#undef ADC12_COMMON
#undef RCC
#define ADC1 (&adc1)
#define ADC2 (&adc2)
#define ADC12_COMMON (&common)
#define RCC (&rcc)
uint32_t IRQ_Save(void) { uint32_t old = mask; mask = 1U; return old; }
void IRQ_Restore(uint32_t old) { mask = old; }
void NVIC_EnableIRQ(uint8_t irq) { assert(irq == 18U); irq_enabled = true; }
void NVIC_DisableIRQ(uint8_t irq) { assert(irq == 18U); irq_enabled = false; }
void NVIC_ClearPendingIRQ(uint8_t irq) { assert(irq == 18U); }
void NVIC_SetPriority(uint8_t irq, uint8_t priority) { assert(irq == 18U && priority == 6U); }
uint32_t Timebase_Millis(void)
{
    /* Only emulate calibration/ADRDY here. Conversion events and W1C effects
     * are injected explicitly; the mock is not an electrical ADC model. */
    if (calibrates) adc1.CR &= ~(1UL << 31);
    if ((adc1.CR & 1U) != 0U && ready) adc1.ISR |= 1U;
    else adc1.ISR &= ~1U;
    uint32_t value = now;
    if (!frozen) ++now;
    return value;
}
#include "../src/drivers/STM32F334_ADC.c"

static unsigned calls;
static ADC_Result last_result;
static uint16_t last_value;
static void completed(ADC_Result result, uint16_t value, void *context)
{
    ADC_Handle_t *h = context;
    assert(mask == 0U);
    ++calls; last_result = result; last_value = value;
    ADC_Process(h);
    assert(!ADC_DeInit(h)); /* Cannot destroy handle during callback. */
    if (result == ADC_TIMEOUT) assert(!ADC_Init(h));
}

static void hardware_reset(void)
{
    /* RCC reset writes have no side effect in ordinary RAM. */
    memset(&adc1, 0, sizeof adc1);
    memset(&adc2, 0, sizeof adc2);
    memset(&common, 0, sizeof common);
}

static void start(ADC_Handle_t *h)
{
    assert(ADC_Start_IT(h, 10U, completed, h));
    adc1.ISR = ADC_READY; /* Apply ISR W1C write from Start. */
}

int main(void)
{
    ADC_Handle_t h = {.pADCx = ADC1, .ADC_Config = {8000000U, 2U, 7U, 6U}};
    ADC_Handle_t other = h;
    assert(!ADC_Init(NULL));
    mask = 1U; assert(!ADC_Init(&h)); assert(mask == 1U); mask = 0U;
    h.ADC_Config.ADC_Channel = 0U; assert(!ADC_Init(&h)); h.ADC_Config.ADC_Channel = 2U;
    adc2.CR = ADC_EN; assert(!ADC_Init(&h)); adc2.CR = 0U;
    assert(ADC_Init(&h) && irq_enabled);
    assert(common.CCR == (2U << 16));
    assert(adc1.SQR1 == (2U << 6) && adc1.SMPR1 == (7U << 6));
    assert(!ADC_Init(&other) && !ADC_Init(&h));
    assert(!ADC_Start_IT(&other, 10U, completed, &other));
    start(&h);
    assert(!ADC_DeInit(&h) && !ADC_Start_IT(&h, 10U, completed, &h));
    adc1.DR = 3456U;
    adc1.CR &= ~ADC_START; /* One-shot conversion completed in hardware. */
    adc1.ISR = ADC_READY | ADC_EOC | ADC_EOS;
    ADC_IRQHandling(&h);
    assert(calls == 0U && h.Done && h.Samples == 1U);
    ADC_IRQHandling(&h);
    ADC_Process(&h);
    assert(calls == 1U && last_result == ADC_OK && last_value == 3456U);
    ADC_Process(&h); assert(calls == 1U);

    start(&h); adc1.CR &= ~ADC_START; adc1.ISR = ADC_EOC | ADC_OVR;
    ADC_IRQHandling(&h); ADC_Process(&h);
    assert(calls == 2U && last_result == ADC_OVERRUN && h.Errors == 1U);

    now = UINT32_MAX - 4U;
    start(&h);
    now = h.StartedMs + 10U;
    adc1.ISR = ADC_READY;
    ADC_Process(&h);
    assert(calls == 3U && last_result == ADC_TIMEOUT && !h.Initialized);
    assert(!irq_enabled && (rcc.AHBENR & ADC_CLOCK) == 0U);
    adc1.ISR = ADC_EOC; adc1.DR = 111U;
    ADC_IRQHandling(&h); assert(calls == 3U); /* Late completion cannot leak. */
    hardware_reset(); assert(ADC_Init(&other)); assert(ADC_DeInit(&other));

    hardware_reset(); calibrates = false;
    assert(!ADC_Init(&h) && !h.Initialized && !irq_enabled);
    hardware_reset(); calibrates = true; ready = false;
    assert(!ADC_Init(&h)); /* ADRDY failure releases owner. */
    hardware_reset(); ready = true; frozen = true;
    assert(!ADC_Init(&h)); /* Bounded even if timebase does not tick. */
    hardware_reset(); frozen = false;
    assert(ADC_Init(&h) && ADC_DeInit(&h));
    assert(mask == 0U);
    puts("ADC: calibration, ready timeout, deferred samples, overrun, late IRQ PASS");
    return 0;
}
