#ifndef STM32F334_TIM_H
#define STM32F334_TIM_H
#include "STM32F334.h"
#include <stdbool.h>

#define TIM_MODE_BASE_IRQ 0U
#define TIM_MODE_PWM_CH1 1U
typedef struct {
    uint32_t TIM_ClockHz; /* Clock timer SAU quy tac APB: APB /1 -> PCLK, /N -> 2*PCLK. */
    uint16_t TIM_Prescaler; /* Gia tri ghi PSC, khong phai he so chia. */
    uint32_t TIM_Period;    /* Gia tri ghi ARR: mot chu ky co ARR+1 tick. */
    uint8_t TIM_Mode;
    uint8_t TIM_IRQPriority;
} TIM_Config_t;
typedef void (*TIM_Callback)(uint32_t elapsed_updates, void *context);
typedef struct {
    TIM_RegDef_t *pTIMx;
    TIM_Config_t TIM_Config;
    /* Runtime chi driver sua; handle static/global den DeInit. */
    uint32_t ClockHz, Period;
    uint16_t Prescaler, DutyPermille;
    uint8_t Mode;
    bool Initialized, Running, ProcessRunning;
    volatile uint32_t Updates, PendingUpdates, DroppedUpdates;
    TIM_Callback Callback;
    void *Context;
} TIM_Handle_t;

/* Bai hoc: TIM3 base IRQ (16-bit ARR), TIM2 PWM channel1 (32-bit ARR).
 * Board tu chon GPIO AF; driver khong doi RCC clock tree hay SysTick.
 * Mot task/main so huu handle. ISR khong goi user callback.
 * f = TIM_ClockHz / ((PSC+1)*(ARR+1)); khong nham clock APB va timer. */
void TIM_PeriClockControl(TIM_RegDef_t *timer, uint8_t enable);
bool TIM_Init(TIM_Handle_t *handle);
bool TIM_DeInit(TIM_Handle_t *handle); /* Stop truoc; khong goi trong callback. */
bool TIM_Start_IT(TIM_Handle_t *handle, TIM_Callback callback, void *context);
bool TIM_PWM_Start(TIM_Handle_t *handle);
bool TIM_PWM_SetDuty(TIM_Handle_t *handle, uint16_t permille); /* 0..1000 = 0..100%. */
bool TIM_Stop(TIM_Handle_t *handle);
void TIM_Process(TIM_Handle_t *handle);
void TIM_IRQHandling(TIM_Handle_t *handle);
uint32_t TIM_GetUpdates(TIM_Handle_t *handle);
#endif
