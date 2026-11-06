#ifndef BOARD_PERIPHERALS_H
#define BOARD_PERIPHERALS_H

#include "STM32F334_GPIO.h"
#include "STM32F334_UART.h"
#include "STM32F334_SPI.h"
#include "STM32F334_CAN.h"
#include "STM32F334_I2C.h"
#include "STM32F334_TIM.h"
#include "STM32F334_ADC.h"
#include <stdbool.h>

/* Handle phai song suot thoi gian giao tiep, ke ca khi main dang lam viec khac.
 * Moi ngoai vi chi co MOT handle duoc Init thanh cong tai mot thoi diem. */
extern UART_Handle_t huart2;
extern SPI_Handle_t hspi1;
extern CAN_Handle_t hcan1;
extern I2C_Handle_t hi2c1;
extern TIM_Handle_t htim3, hpwm2;
extern ADC_Handle_t hadc1;

void Board_LEDInit(void);
bool Board_UARTInit(void);
bool Board_SPIInit(void);
bool Board_CANInit(void);
bool Board_I2CInit(void);
bool Board_TimerInit(void);
bool Board_PWMInit(void);
bool Board_ADCInit(void);
void Board_GPIOInputInit(void);

/* Board_*Init chi goi luc startup hoac sau DeInit thanh cong.
 * Goi lai khi handle dang Initialized se tra false ma khong cham pin/CS. */

#endif
