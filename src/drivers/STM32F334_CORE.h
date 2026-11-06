#ifndef STM32F334_CORE_H
#define STM32F334_CORE_H

#include "STM32F334.h"
#include <stdbool.h>
#include <stddef.h>

/* NVIC la tai nguyen chung cua CPU, khong thuoc rieng driver GPIO.
 * Priority 0 la cao nhat; STM32F334 thuc hien 4 bit priority (0..15). */
uint32_t IRQ_Save(void);
void IRQ_Restore(uint32_t saved_primask);
void NVIC_EnableIRQ(uint8_t irq);
void NVIC_DisableIRQ(uint8_t irq);
void NVIC_ClearPendingIRQ(uint8_t irq);
void NVIC_SetPriority(uint8_t irq, uint8_t priority);

/* SysTick tao moc thoi gian, khong doi cau hinh RCC/PLL.
 * Vi du hien tai giu HSI reset: CPU, APB1, APB2 deu la 8 MHz. */
bool Timebase_Init(uint32_t core_hz);
uint32_t Timebase_Millis(void);

#endif
