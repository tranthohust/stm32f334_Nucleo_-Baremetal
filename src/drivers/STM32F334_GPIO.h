#ifndef INC_STM32F334GPIO_H_
#define INC_STM32F334GPIO_H_
#include "STM32F334.h"
#include <stdint.h>
//speccifies for GPIO pin to be configured
typedef struct
{
    uint8_t GPIO_PinNumber;        //specifies the GPIO pin to be configured
    uint8_t GPIO_PinMode;          //specifies the operating mode for the selected pin
    uint8_t GPIO_PinSpeed;         //specifies the speed for the selected pin
    uint8_t GPIO_PinPuPdControl;   //specifies the Pull-up or Pull-Down activation for the selected pin
    uint8_t GPIO_PinOPType;        //specifies the operating output type for the selected pin
    uint8_t GPIO_PinAltFunMode;    //specifies the alternate function mode for the selected pin
}GPIO_PinConfig_t;
//setting for GPIO handle structure
typedef struct
{
    GPIO_PinConfig_t GPIO_PinConfig; //pin configuration settings
    GPIO_RegDef_t *pGPIOx;           //pointer to hold the base address of the GPIO port to which the pin belongs
}GPIO_Handle_t;
#define GPIO_PIN_NO_0  0
#define GPIO_PIN_NO_1  1
#define GPIO_PIN_NO_2  2
#define GPIO_PIN_NO_3  3
#define GPIO_PIN_NO_4  4
#define GPIO_PIN_NO_5  5
#define GPIO_PIN_NO_6  6
#define GPIO_PIN_NO_7  7
#define GPIO_PIN_NO_8  8
#define GPIO_PIN_NO_9  9
#define GPIO_PIN_NO_10 10
#define GPIO_PIN_NO_11 11
#define GPIO_PIN_NO_12 12
#define GPIO_PIN_NO_13 13
#define GPIO_PIN_NO_14 14
#define GPIO_PIN_NO_15 15
//GPIO mode settings
#define GPIO_MODE_IN     0
#define GPIO_MODE_OUT    1
#define GPIO_MODE_ALTFN  2
#define GPIO_MODE_ANALOG 3
#define GPIO_MODE_IT_FT  4
#define GPIO_MODE_IT_RT  5
#define GPIO_MODE_IT_RFT 6

//gpio output type settings
#define GPIO_OP_TYPE_PP 0
#define GPIO_OP_TYPE_OD 1
//gpio speed settings
#define GPIO_SPEED_LOW    0
#define GPIO_SPEED_MEDIUM 1
#define GPIO_SPEED_HIGH    2
//gpio pull-up/pull-down settings
#define GPIO_NO_PUPD 0
#define GPIO_PIN_PU 1
#define GPIO_PIN_PD 2
#define Reserve 3
//gpio alternate function settings
#define GPIO_AF0 0
#define GPIO_AF1 1
#define GPIO_AF2 2
#define GPIO_AF3 3
#define GPIO_AF4 4
#define GPIO_AF5 5
#define GPIO_AF6 6
#define GPIO_AF7 7
#define GPIO_AF8 8
#define GPIO_AF9 9
#define GPIO_AF10 10
#define GPIO_AF11 11
#define GPIO_AF12 12
#define GPIO_AF13 13
#define GPIO_AF14 14
#define GPIO_AF15 15
//


//function prototypes for GPIO driver
/*
1. Peripheral Clock Setup
2. Init 
3. De-init (reset)
4. Data read
5. Data Write
6. IRQ Configuration and ISR Handling
*/
void GPIO_PeriClockControl(GPIO_RegDef_t *pGPIOx, uint8_t EnorDi);
void GPIO_Init(GPIO_Handle_t *pGPIOHandle);
void GPIO_DeInit(GPIO_RegDef_t *pGPIOx);
uint8_t GPIO_ReadFromInputPin(GPIO_RegDef_t *pGPIOx, uint8_t PinNumber);
uint16_t GPIO_ReadFromInputPort(GPIO_RegDef_t *pGPIOx);
void GPIO_WriteToOutputPin(GPIO_RegDef_t *pGPIOx, uint8_t PinNumber, uint8_t Value);
void GPIO_WriteToOutputPort(GPIO_RegDef_t *pGPIOx, uint16_t Value);
void GPIO_ToggleOutputPin(GPIO_RegDef_t *pGPIOx, uint8_t PinNumber);
void GPIO_IRQConfig(uint8_t IRQNumber, uint8_t IRQPriority, uint8_t EnorDi);
void GPIO_IRQPriorityConfig(uint8_t IRQNumber, uint32_t IRQPriority);
void GPIO_IRQHandling(uint8_t PinNumber);

#endif /* INC_STM32F334GPIO_H_ */
