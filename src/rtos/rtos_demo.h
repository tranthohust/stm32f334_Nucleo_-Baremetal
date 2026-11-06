#ifndef RTOS_DEMO_H
#define RTOS_DEMO_H

#include "STM32F334_I2C.h"
#include <stdbool.h>
#include <stdint.h>

typedef struct {
    uint8_t address, reg, value;
    bool accepted;
    I2C_Result result;
} RTOS_I2CResponse;

typedef struct {
    uint32_t requests, request_dropped, responses, response_dropped;
    uint32_t irq_wakeups;
    uint32_t comm_stack_free, worker_stack_free, producer_stack_free;
    bool producer_enabled, i2c_ready;
    I2C_Stats i2c;
} RTOS_DemoStats;

void RTOS_Start(void);
/* Cac API nay chi goi tu task, khong goi trong IRQ/PRIMASK critical section. */
bool RTOS_QueueI2CRead(uint8_t address, uint8_t reg);
bool RTOS_TakeI2CResult(RTOS_I2CResponse *response);
void RTOS_SetProducerEnabled(bool enabled);
void RTOS_GetStats(RTOS_DemoStats *out);
/* Chi EV/ER ISR priority6 goi, SAU khi driver da khoi phuc PRIMASK. */
void RTOS_I2CNotifyFromISR(void);

#endif
