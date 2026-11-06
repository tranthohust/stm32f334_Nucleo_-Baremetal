#include "STM32F334_TIM.h"
#include "STM32F334_CORE.h"
#include <stddef.h>

#define TIM_CEN 1U
#define TIM_ARPE (1U << 7)
#define TIM_UIF 1U
#define TIM_UIE 1U
#define TIM_UG 1U
#define TIM_OC1PE (1U << 3)
#define TIM_PWM1 (6U << 4)
#define TIM_FORCE_LOW (4U << 4)
#define TIM_CC1E 1U
#define TIM3_IRQ 29U
_Static_assert(offsetof(TIM_RegDef_t, PSC) == 0x28U, "TIM PSC offset");
_Static_assert(offsetof(TIM_RegDef_t, CCR1) == 0x34U, "TIM CCR1 offset");
static TIM_Handle_t *owner2, *owner3;

static bool owned(const TIM_Handle_t *h)
{
    return h != NULL && ((h == owner2 && h->pTIMx == TIM2) ||
                         (h == owner3 && h->pTIMx == TIM3)) && h->Initialized;
}

void TIM_PeriClockControl(TIM_RegDef_t *timer, uint8_t enable)
{
    uint32_t bit;
    if (timer == TIM2) bit = 1U;
    else if (timer == TIM3) bit = 2U;
    else return;
    uint32_t key = IRQ_Save();
    if (enable == ENABLE) RCC->APB1ENR |= bit;
    else if (enable == DISABLE) RCC->APB1ENR &= ~bit;
    (void)RCC->APB1ENR;
    IRQ_Restore(key);
}

static void reset(TIM_RegDef_t *timer)
{
    uint32_t bit = timer == TIM2 ? 1U : 2U;
    RCC->APB1RSTR |= bit;
    RCC->APB1RSTR &= ~bit;
}

bool TIM_Init(TIM_Handle_t *h)
{
    if (h == NULL || h->TIM_Config.TIM_ClockHz == 0U ||
        h->TIM_Config.TIM_IRQPriority > 15U) return false;
    bool base = h->pTIMx == TIM3 && h->TIM_Config.TIM_Mode == TIM_MODE_BASE_IRQ;
    bool pwm = h->pTIMx == TIM2 && h->TIM_Config.TIM_Mode == TIM_MODE_PWM_CH1;
    /* PWM 100% can CCR=ARR+1, vi vay khong cho ARR=UINT32_MAX. */
    if ((!base && !pwm) || (base && h->TIM_Config.TIM_Period > 65535U) ||
        (pwm && h->TIM_Config.TIM_Period == UINT32_MAX)) return false;
    uint32_t key = IRQ_Save();
    if (h == owner2 || h == owner3 || (base ? owner3 : owner2) != NULL) {
        IRQ_Restore(key); return false;
    }
    TIM_PeriClockControl(h->pTIMx, ENABLE);
    reset(h->pTIMx);
    h->Mode = h->TIM_Config.TIM_Mode;
    h->ClockHz = h->TIM_Config.TIM_ClockHz;
    h->Prescaler = h->TIM_Config.TIM_Prescaler;
    h->Period = h->TIM_Config.TIM_Period;
    h->Updates = h->PendingUpdates = h->DroppedUpdates = 0U;
    h->DutyPermille = 0U;
    h->Running = h->ProcessRunning = false;
    h->Callback = NULL;
    h->Context = NULL;
    h->pTIMx->CR1 = TIM_ARPE;
    h->pTIMx->DIER = 0U;
    h->pTIMx->CCER = 0U;
    h->pTIMx->PSC = h->Prescaler;
    h->pTIMx->ARR = h->Period;
    h->pTIMx->CNT = 0U;
    if (pwm) {
        h->pTIMx->CCMR1 = TIM_OC1PE | TIM_FORCE_LOW;
        h->pTIMx->CCR1 = 0U;
    }
    h->pTIMx->EGR = TIM_UG; /* Nap PSC/ARR preload vao active registers. */
    h->pTIMx->SR = 0U;     /* Khong dem UG cua Init nhu update that. */
    h->Initialized = true;
    if (base) {
        owner3 = h;
        NVIC_DisableIRQ(TIM3_IRQ);
        NVIC_ClearPendingIRQ(TIM3_IRQ);
        NVIC_SetPriority(TIM3_IRQ, h->TIM_Config.TIM_IRQPriority);
    } else owner2 = h;
    IRQ_Restore(key);
    return true;
}

bool TIM_Start_IT(TIM_Handle_t *h, TIM_Callback callback, void *context)
{
    uint32_t key = IRQ_Save();
    if (!owned(h) || h->Mode != TIM_MODE_BASE_IRQ || h->Running || h->ProcessRunning) {
        IRQ_Restore(key); return false;
    }
    h->Callback = callback;
    h->Context = context;
    h->PendingUpdates = 0U;
    h->pTIMx->CNT = 0U;
    h->pTIMx->EGR = TIM_UG;
    h->pTIMx->SR = 0U;
    NVIC_ClearPendingIRQ(TIM3_IRQ);
    h->pTIMx->DIER = TIM_UIE;
    h->Running = true;
    NVIC_EnableIRQ(TIM3_IRQ);
    h->pTIMx->CR1 |= TIM_CEN;
    IRQ_Restore(key);
    return true;
}

bool TIM_PWM_SetDuty(TIM_Handle_t *h, uint16_t permille)
{
    uint32_t key = IRQ_Save();
    if (!owned(h) || h->Mode != TIM_MODE_PWM_CH1 || permille > 1000U) {
        IRQ_Restore(key); return false;
    }
    uint32_t compare = (uint32_t)(((uint64_t)h->Period + 1U) * permille / 1000U);
    h->pTIMx->CCR1 = compare;
    h->DutyPermille = permille;
    /* Dang chay: preload CCR chi doi o update tu nhien, khong reset chu ky.
     * Dung: UG nap gia tri cho lan Start tiep theo. */
    if (!h->Running) { h->pTIMx->EGR = TIM_UG; h->pTIMx->SR = 0U; }
    IRQ_Restore(key);
    return true;
}

bool TIM_PWM_Start(TIM_Handle_t *h)
{
    uint32_t key = IRQ_Save();
    if (!owned(h) || h->Mode != TIM_MODE_PWM_CH1 || h->Running) {
        IRQ_Restore(key); return false;
    }
    h->pTIMx->CCMR1 = TIM_OC1PE | TIM_PWM1;
    h->pTIMx->CNT = 0U;
    h->pTIMx->EGR = TIM_UG;
    h->pTIMx->SR = 0U;
    h->pTIMx->CCER = TIM_CC1E;
    h->Running = true;
    h->pTIMx->CR1 |= TIM_CEN;
    IRQ_Restore(key);
    return true;
}

bool TIM_Stop(TIM_Handle_t *h)
{
    uint32_t key = IRQ_Save();
    if (!owned(h)) { IRQ_Restore(key); return false; }
    h->pTIMx->CR1 &= ~TIM_CEN;
    h->pTIMx->DIER = 0U;
    if (h->Mode == TIM_MODE_PWM_CH1) {
        /* Force inactive giu OC1 low khi dung, khong de gia tri compare cu high. */
        h->pTIMx->CCMR1 = TIM_FORCE_LOW;
        h->pTIMx->CCER = TIM_CC1E;
    } else {
        NVIC_DisableIRQ(TIM3_IRQ);
        NVIC_ClearPendingIRQ(TIM3_IRQ);
    }
    h->pTIMx->SR = 0U;
    h->PendingUpdates = 0U; /* Stop chu dong bo callback chua xu ly. */
    h->Running = false;
    IRQ_Restore(key);
    return true;
}

bool TIM_DeInit(TIM_Handle_t *h)
{
    uint32_t key = IRQ_Save();
    if (!owned(h) || h->Running || h->ProcessRunning) { IRQ_Restore(key); return false; }
    reset(h->pTIMx);
    TIM_PeriClockControl(h->pTIMx, DISABLE);
    if (h == owner2) owner2 = NULL;
    else owner3 = NULL;
    h->Initialized = false;
    IRQ_Restore(key);
    return true;
}

void TIM_IRQHandling(TIM_Handle_t *h)
{
    uint32_t key = IRQ_Save();
    if (owned(h) && h->Mode == TIM_MODE_BASE_IRQ && h->Running &&
        (h->pTIMx->DIER & TIM_UIE) != 0U && (h->pTIMx->SR & TIM_UIF) != 0U) {
        h->pTIMx->SR = ~TIM_UIF; /* TIM SR: write-ZERO-to-clear, khac ADC ISR. */
        ++h->Updates;
        if (h->PendingUpdates != UINT32_MAX) ++h->PendingUpdates;
        else ++h->DroppedUpdates;
    }
    IRQ_Restore(key);
}

void TIM_Process(TIM_Handle_t *h)
{
    uint32_t key = IRQ_Save();
    if (!owned(h) || h->ProcessRunning) { IRQ_Restore(key); return; }
    h->ProcessRunning = true;
    uint32_t count = h->PendingUpdates;
    h->PendingUpdates = 0U;
    TIM_Callback callback = h->Callback;
    void *context = h->Context;
    IRQ_Restore(key);
    if (count != 0U && callback != NULL) callback(count, context);
    h->ProcessRunning = false;
}

uint32_t TIM_GetUpdates(TIM_Handle_t *h)
{
    return owned(h) ? h->Updates : 0U;
}
