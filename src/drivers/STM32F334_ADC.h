#ifndef STM32F334_ADC_H
#define STM32F334_ADC_H
#include "STM32F334.h"
#include <stdbool.h>

#define ADC_SAMPLE_601CYCLES_5 7U
typedef struct {
    uint32_t ADC_CoreClockHz; /* Bai hoc giu HSI8/HCLK8, ADC synchronous HCLK/2. */
    uint8_t ADC_Channel;     /* Single-ended regular external channel 1..15. */
    uint8_t ADC_SampleTime;  /* SMPR encoding 0..7; board phai chon pin dung channel. */
    uint8_t ADC_IRQPriority;
} ADC_Config_t;
typedef enum { ADC_OK, ADC_TIMEOUT, ADC_OVERRUN } ADC_Result;
typedef void (*ADC_Callback)(ADC_Result result, uint16_t sample, void *context);
typedef struct {
    ADC_RegDef_t *pADCx;
    ADC_Config_t ADC_Config;
    bool Initialized, Busy, Done, ProcessRunning;
    uint32_t StartedMs, TimeoutMs, Samples, Errors;
    uint16_t Value;
    ADC_Result Result;
    ADC_Callback Callback;
    void *Context;
} ADC_Handle_t;

/* ADC1 only, 12-bit right-aligned, software-trigger single conversion + IRQ.
 * Init can doi ngan co timeout: regulator -> calibration -> ADRDY.
 * Timebase_Init phai chay truoc, khong goi Init khi IRQ bi mask.
 * Driver DOC QUYEN ADC12 common/reset/clock; khong dung ADC2 dong thoi.
 * GPIO analog do board cau hinh, khong dung DMA/scan/continuous trong bai nay. */
void ADC_PeriClockControl(ADC_RegDef_t *adc, uint8_t enable);
bool ADC_Init(ADC_Handle_t *handle);
bool ADC_DeInit(ADC_Handle_t *handle); /* Idle only, discards pending result. */
bool ADC_Start_IT(ADC_Handle_t *handle, uint32_t timeout_ms,
                  ADC_Callback callback, void *context);
void ADC_IRQHandling(ADC_Handle_t *handle);
void ADC_Process(ADC_Handle_t *handle);
/* Timeout reset ADC12 va nha owner, Initialized=false; can Init lai.
 * Callback luon chay trong Process, ke ca khi timeout. Chi mot task so huu. */
#endif
