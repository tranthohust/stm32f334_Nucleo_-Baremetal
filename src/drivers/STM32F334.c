#include "STM32F334_GPIO.h"
#include "STM32F334_CORE.h"

void GPIO_PeriClockControl(GPIO_RegDef_t *pGPIOx, uint8_t EnorDi){
    if(EnorDi == ENABLE){
        if(pGPIOx == GPIOA){
            GPIOA_PCLK_EN();
        }else if(pGPIOx == GPIOB){
            GPIOB_PCLK_EN();
        }else if(pGPIOx == GPIOC){
            GPIOC_PCLK_EN();
        }else if(pGPIOx == GPIOD){
            GPIOD_PCLK_EN();
        }else if(pGPIOx == GPIOF){
            GPIOF_PCLK_EN();
        }
}
    else{
        if(pGPIOx == GPIOA){
            GPIOA_PCLK_DI();
        }else if(pGPIOx == GPIOB){
            GPIOB_PCLK_DI();
        }else if(pGPIOx == GPIOC){
            GPIOC_PCLK_DI();
        }else if(pGPIOx == GPIOD){
            GPIOD_PCLK_DI();
        }else if(pGPIOx == GPIOF){
            GPIOF_PCLK_DI();
        }
    }
}

void GPIO_Init(GPIO_Handle_t *pGPIOHandle){
    
        //1. Configure the mode of GPIO pin

        //2. Configure the speed
       // 3. Configure the pull-up/pull-down settings
       // 4. Configure the output type
      //  5. Configure the alternate functionality
    
    uint32_t temp = 0;
    if(pGPIOHandle ->GPIO_PinConfig.GPIO_PinMode <= GPIO_MODE_ANALOG)//normal mode
    {

        temp = ((uint32_t)pGPIOHandle->GPIO_PinConfig.GPIO_PinMode << (2U * pGPIOHandle->GPIO_PinConfig.GPIO_PinNumber));
        pGPIOHandle->pGPIOx->MODER &= ~(0x3UL << (2U * pGPIOHandle->GPIO_PinConfig.GPIO_PinNumber)); //clearing
        pGPIOHandle->pGPIOx->MODER |= temp;//setting 
    }
    else
    {
        //interrupt mode
        /* EXTI can bat canh tren pin input; IMR1 moi cho phep IRQ di qua. */
        pGPIOHandle->pGPIOx->MODER &=
            ~(3UL << (2U * pGPIOHandle->GPIO_PinConfig.GPIO_PinNumber));
        if(pGPIOHandle ->GPIO_PinConfig.GPIO_PinMode == GPIO_MODE_IT_FT){
            EXIT ->FTSR1 |= (1UL << pGPIOHandle -> GPIO_PinConfig.GPIO_PinNumber);
            EXIT ->RTSR1 &=  ~(1UL << pGPIOHandle ->GPIO_PinConfig.GPIO_PinNumber);
        }
        else if(pGPIOHandle -> GPIO_PinConfig.GPIO_PinMode == GPIO_MODE_IT_RT){
            EXIT ->RTSR1 |= (1UL << pGPIOHandle -> GPIO_PinConfig.GPIO_PinNumber);
            EXIT ->FTSR1 &=  ~(1UL << pGPIOHandle ->GPIO_PinConfig.GPIO_PinNumber);
        }
        else if(pGPIOHandle -> GPIO_PinConfig.GPIO_PinMode == GPIO_MODE_IT_RFT){
            EXIT ->RTSR1 |= (1UL << pGPIOHandle -> GPIO_PinConfig.GPIO_PinNumber);
            EXIT ->FTSR1 |= (1UL << pGPIOHandle -> GPIO_PinConfig.GPIO_PinNumber);
        }
        uint8_t temp1 = pGPIOHandle -> GPIO_PinConfig.GPIO_PinNumber / 4;
        uint8_t temp2 = pGPIOHandle -> GPIO_PinConfig.GPIO_PinNumber % 4;
        uint8_t portcode = GPIO_BASEADDR_TO_CODE(pGPIOHandle -> pGPIOx);
        SYSCFG_PCLK_EN();
        SYSCFG ->EXTICR[temp1] &= ~(0xFUL << (temp2 * 4U));
        SYSCFG ->EXTICR[temp1] |= ((uint32_t)portcode << (temp2 * 4U));
        EXIT->PR1 = 1UL << pGPIOHandle->GPIO_PinConfig.GPIO_PinNumber;
        EXIT->IMR1 |= 1UL << pGPIOHandle->GPIO_PinConfig.GPIO_PinNumber;
    }
    //2. Configure the speed
    temp = ((uint32_t)pGPIOHandle->GPIO_PinConfig.GPIO_PinSpeed << (2U * pGPIOHandle->GPIO_PinConfig.GPIO_PinNumber));
    pGPIOHandle -> pGPIOx ->OSPEEDR &= ~(0x3UL << (2U * pGPIOHandle -> GPIO_PinConfig.GPIO_PinNumber)); //clearing
    pGPIOHandle -> pGPIOx ->OSPEEDR |= temp;
    //3. Configure the pull-up/pull-down settings
    temp = ((uint32_t)pGPIOHandle->GPIO_PinConfig.GPIO_PinPuPdControl << (2U * pGPIOHandle->GPIO_PinConfig.GPIO_PinNumber));
    pGPIOHandle -> pGPIOx ->PUPDR &= ~(0x3UL << (2U * pGPIOHandle->GPIO_PinConfig.GPIO_PinNumber));
    pGPIOHandle -> pGPIOx ->PUPDR |= temp;
    //4. Configure the output type
    temp  = ((uint32_t)pGPIOHandle->GPIO_PinConfig.GPIO_PinOPType << pGPIOHandle->GPIO_PinConfig.GPIO_PinNumber);
    pGPIOHandle ->pGPIOx -> OTYPER  &= ~(1U << pGPIOHandle ->GPIO_PinConfig.GPIO_PinNumber);
    pGPIOHandle -> pGPIOx ->OTYPER |= temp;
    //5. Configure the alternate functionality
    if(pGPIOHandle ->GPIO_PinConfig.GPIO_PinMode == GPIO_MODE_ALTFN)
    {
        //configure the alternate function registers
        uint8_t temp1, temp2;
        //config alternate function register
        temp1 = pGPIOHandle ->GPIO_PinConfig.GPIO_PinNumber /8;
        temp2 = pGPIOHandle ->GPIO_PinConfig.GPIO_PinNumber %8;
        pGPIOHandle ->pGPIOx ->AFR[temp1] &= ~(0xFUL << (4U * temp2));
        pGPIOHandle ->pGPIOx ->AFR[temp1] |= ((uint32_t)pGPIOHandle->GPIO_PinConfig.GPIO_PinAltFunMode << (4U * temp2));
    }
    
}    

void GPIO_DeInit(GPIO_RegDef_t *pGPIOx){

    if(pGPIOx == GPIOA){
        GPIOA_REG_RESET();
    }else if(pGPIOx == GPIOB){
        GPIOB_REG_RESET();
    }else if(pGPIOx == GPIOC){
        GPIOC_REG_RESET();
    }else if(pGPIOx == GPIOD){
        GPIOD_REG_RESET();
    }else if(pGPIOx == GPIOF){
        GPIOF_REG_RESET();
    }
}
uint8_t GPIO_ReadFromInputPin(GPIO_RegDef_t *pGPIOx, uint8_t PinNumber)
{
    //dich phai de lay gia tri cua pin tu thanh ghi IDR
    uint8_t value = (uint8_t)((pGPIOx->IDR >> PinNumber) & 1U);
    return value;
}
uint16_t GPIO_ReadFromInputPort(GPIO_RegDef_t *pGPIOx){
    uint16_t value = (uint16_t)pGPIOx->IDR;
    return value;
}
void GPIO_WriteToOutputPin(GPIO_RegDef_t *pGPIOx, uint8_t PinNumber, uint8_t Value){
    if (pGPIOx == NULL || PinNumber > 15U) return;
    /* Mot lan ghi BSRR set/reset dung pin, khong RMW ODR.
     * Neu ISR thay doi pin khac trong cung port, thay doi do khong bi mat. */
    pGPIOx->BSRR = 1UL << ((uint32_t)PinNumber +
                         (Value == GPIO_PIN_SET ? 0U : 16U));
}
void GPIO_WriteToOutputPort(GPIO_RegDef_t *pGPIOx, uint16_t Value){
    pGPIOx -> ODR = Value;

}
void GPIO_ToggleOutputPin(GPIO_RegDef_t *pGPIOx, uint8_t PinNumber){
    /* Toggle van can doc trang thai cu: cac caller cung port phai dong bo. */
    if (pGPIOx != NULL && PinNumber < 16U)
        pGPIOx->ODR ^= (1UL << PinNumber);
}
void GPIO_IRQConfig(uint8_t IRQNumber, uint8_t IRQPriority, uint8_t EnorDi){
    /* Giu API cu cho bai GPIO; phan NVIC thuc su dung chung cho moi driver. */
    if (IRQNumber >= 96U || IRQPriority >= 16U) return;
    if (EnorDi == ENABLE) {
        NVIC_SetPriority(IRQNumber, IRQPriority);
        NVIC_EnableIRQ(IRQNumber);
    } else {
        NVIC_DisableIRQ(IRQNumber);
    }
}
void GPIO_IRQPriorityConfig(uint8_t IRQNumber, uint32_t IRQPriority){
    if (IRQPriority < 16U)
        NVIC_SetPriority(IRQNumber, (uint8_t)IRQPriority);
}
void GPIO_IRQHandling(uint8_t PinNumber){
    if (PinNumber < 16U && (EXIT->PR1 & (1UL << PinNumber)) != 0U) {
        /* PR1 la W1C: |= co the vo tinh xoa pending cua pin KHAC. */
        EXIT->PR1 = 1UL << PinNumber;
    }
}
