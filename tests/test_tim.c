#include "../src/drivers/STM32F334_TIM.h"
#include "../src/drivers/STM32F334_CORE.h"
#include <assert.h>
#include <stdio.h>

static TIM_RegDef_t timer2, timer3;
static RCC_RegDef_t rcc;
static uint32_t mask;
static bool irq_enabled;
#undef TIM2
#undef TIM3
#undef RCC
#define TIM2 (&timer2)
#define TIM3 (&timer3)
#define RCC (&rcc)
uint32_t IRQ_Save(void) { uint32_t old = mask; mask = 1U; return old; }
void IRQ_Restore(uint32_t old) { mask = old; }
void NVIC_EnableIRQ(uint8_t irq) { assert(irq == 29U); irq_enabled = true; }
void NVIC_DisableIRQ(uint8_t irq) { assert(irq == 29U); irq_enabled = false; }
void NVIC_ClearPendingIRQ(uint8_t irq) { assert(irq == 29U); }
void NVIC_SetPriority(uint8_t irq, uint8_t priority) { assert(irq == 29U && priority == 6U); }
#include "../src/drivers/STM32F334_TIM.c"

static uint32_t calls, last_count;
static void elapsed(uint32_t count, void *context)
{
    TIM_Handle_t *h = context;
    assert(mask == 0U);
    ++calls;
    last_count = count;
    TIM_Process(h); /* Recursive Process must not call twice. */
    assert(!TIM_DeInit(h));
}

int main(void)
{
    TIM_Handle_t base = {.pTIMx = TIM3, .TIM_Config = {8000000U, 7999U, 99U, TIM_MODE_BASE_IRQ, 6U}};
    TIM_Handle_t pwm = {.pTIMx = TIM2, .TIM_Config = {8000000U, 7U, 999U, TIM_MODE_PWM_CH1, 6U}};
    TIM_Handle_t other = base;
    assert(!TIM_Init(NULL));
    assert(TIM_Init(&base) && !irq_enabled);
    assert(timer3.PSC == 7999U && timer3.ARR == 99U && timer3.SR == 0U);
    assert(!TIM_Init(&other) && !TIM_Init(&base));
    assert(TIM_Start_IT(&base, elapsed, &base) && irq_enabled);
    /* An existing owner cannot be rebound to the other timer through Init. */
    base.pTIMx = TIM2;
    base.TIM_Config.TIM_Mode = TIM_MODE_PWM_CH1;
    assert(!TIM_Init(&base));
    base.pTIMx = TIM3;
    base.TIM_Config.TIM_Mode = TIM_MODE_BASE_IRQ;
    assert(TIM_Init(&pwm)); /* Separate handles can operate concurrently. */
    assert(TIM_PWM_SetDuty(&pwm, 250U) && TIM_PWM_Start(&pwm));
    assert(TIM_GetUpdates(&base) == 0U); /* UG startup not counted. */
    assert(!TIM_DeInit(&base) && !TIM_Start_IT(&base, elapsed, &base));
    timer3.SR = 3U;
    TIM_IRQHandling(&base);
    assert(timer3.SR == (uint32_t)~1U && calls == 0U); /* W0C, unlike ADC W1C. */
    timer3.SR = 1U; TIM_IRQHandling(&base);
    assert(TIM_GetUpdates(&base) == 2U);
    assert(pwm.Running && timer2.CCR1 == 250U);
    TIM_Process(&base);
    assert(calls == 1U && last_count == 2U);
    TIM_Process(&base); assert(calls == 1U);
    base.PendingUpdates = UINT32_MAX;
    timer3.SR = 1U; TIM_IRQHandling(&base);
    assert(base.DroppedUpdates == 1U && base.PendingUpdates == UINT32_MAX);
    assert(TIM_Stop(&base) && !irq_enabled && base.PendingUpdates == 0U);
    timer3.SR = 1U;
    uint32_t updates = base.Updates;
    TIM_IRQHandling(&base); assert(base.Updates == updates);
    assert(TIM_DeInit(&base) && !TIM_Stop(&base));
    assert(TIM_Init(&other));
    assert(TIM_DeInit(&other));

    assert(TIM_Stop(&pwm) && TIM_DeInit(&pwm));
    assert(TIM_Init(&pwm));
    assert(!TIM_Start_IT(&pwm, elapsed, NULL));
    assert(TIM_PWM_SetDuty(&pwm, 500U) && timer2.CCR1 == 500U);
    assert(TIM_PWM_Start(&pwm) && (timer2.CCER & 1U) != 0U);
    assert(!TIM_DeInit(&pwm));
    timer2.EGR = 0U;
    assert(TIM_PWM_SetDuty(&pwm, 1000U) && timer2.CCR1 == 1000U);
    assert(timer2.EGR == 0U); /* Natural update applies preload, no phase reset. */
    assert(TIM_PWM_SetDuty(&pwm, 0U) && timer2.CCR1 == 0U);
    assert(!TIM_PWM_SetDuty(&pwm, 1001U) && timer2.CCR1 == 0U);
    assert(TIM_Stop(&pwm) && (timer2.CCMR1 & (7U << 4)) == (4U << 4));
    assert(TIM_DeInit(&pwm));
    pwm.TIM_Config.TIM_Period = UINT32_MAX;
    assert(!TIM_Init(&pwm));
    pwm.TIM_Config.TIM_Period = UINT32_MAX - 1U;
    assert(TIM_Init(&pwm));
    assert(TIM_PWM_SetDuty(&pwm, 1000U) && timer2.CCR1 == UINT32_MAX);
    assert(TIM_DeInit(&pwm));
    assert(mask == 0U);
    puts("TIM/PWM: ownership, deferred IRQ, W0C, preload and 0/100 percent PASS");
    return 0;
}
