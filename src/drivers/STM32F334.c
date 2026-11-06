#include "STM32F334_GPIO.h"
#define NO_PR_BITS_IMPLEMENTED 4U

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

        temp = (pGPIOHandle->GPIO_PinConfig.GPIO_PinMode << (2 * pGPIOHandle->GPIO_PinConfig.GPIO_PinNumber));
        pGPIOHandle->pGPIOx->MODER &= ~(0x3 << (2 * pGPIOHandle->GPIO_PinConfig.GPIO_PinNumber)); //clearing
        pGPIOHandle->pGPIOx->MODER |= temp;//setting 
    }
    else
    {
        //interrupt mode
        if(pGPIOHandle ->GPIO_PinConfig.GPIO_PinMode == GPIO_MODE_IT_FT){
            EXIT ->FTSR1 |= (1 << pGPIOHandle -> GPIO_PinConfig.GPIO_PinNumber);
            EXIT ->RTSR1 &=  ~( 1<< pGPIOHandle ->GPIO_PinConfig.GPIO_PinNumber);
        }
        else if(pGPIOHandle -> GPIO_PinConfig.GPIO_PinMode == GPIO_MODE_IT_RT){
            EXIT ->RTSR1 |= (1 << pGPIOHandle -> GPIO_PinConfig.GPIO_PinNumber);
            EXIT ->FTSR1 &=  ~( 1<< pGPIOHandle ->GPIO_PinConfig.GPIO_PinNumber);
        }
        else if(pGPIOHandle -> GPIO_PinConfig.GPIO_PinMode == GPIO_MODE_IT_RFT){
            EXIT ->RTSR1 |= (1 << pGPIOHandle -> GPIO_PinConfig.GPIO_PinNumber);
            EXIT ->FTSR1 |= (1 << pGPIOHandle -> GPIO_PinConfig.GPIO_PinNumber);
        }
        uint8_t temp1 = pGPIOHandle -> GPIO_PinConfig.GPIO_PinNumber / 4;
        uint8_t temp2 = pGPIOHandle -> GPIO_PinConfig.GPIO_PinNumber % 4;
        uint8_t portcode = GPIO_BASEADDR_TO_CODE(pGPIOHandle -> pGPIOx);
        SYSCFG_PCLK_EN();
        SYSCFG ->EXTICR[temp1] &= ~(0xF << (temp2 * 4));
        SYSCFG ->EXTICR[temp1] |= (portcode << (temp2 * 4));
    }
    //2. Configure the speed
    temp = (pGPIOHandle-> GPIO_PinConfig.GPIO_PinSpeed << (2 * pGPIOHandle->GPIO_PinConfig.GPIO_PinNumber));
    pGPIOHandle -> pGPIOx ->OSPEEDR &= ~(0x3 << (2 * pGPIOHandle -> GPIO_PinConfig.GPIO_PinNumber)); //clearing
    pGPIOHandle -> pGPIOx ->OSPEEDR |= temp;
    //3. Configure the pull-up/pull-down settings
    temp = (pGPIOHandle ->GPIO_PinConfig.GPIO_PinPuPdControl <<(2*pGPIOHandle -> GPIO_PinConfig.GPIO_PinNumber));
    pGPIOHandle -> pGPIOx ->PUPDR &= ~(0x3 <<(2* pGPIOHandle -> GPIO_PinConfig.GPIO_PinNumber));
    pGPIOHandle -> pGPIOx ->PUPDR |= temp;
    //4. Configure the output type
    temp  = (pGPIOHandle -> GPIO_PinConfig.GPIO_PinOPType << pGPIOHandle -> GPIO_PinConfig.GPIO_PinNumber);
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
        pGPIOHandle ->pGPIOx ->AFR[temp1] &= ~(0xF << (4 * temp2));
        pGPIOHandle ->pGPIOx ->AFR[temp1] |= (pGPIOHandle -> GPIO_PinConfig.GPIO_PinAltFunMode << (4 * temp2));
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
    if(Value == GPIO_PIN_SET){
        //write 1 to the output data register at the bit field corresponding to the pin number
        pGPIOx->ODR |= (1 << PinNumber);}
        else{
         pGPIOx->ODR &= ~( 1 << PinNumber);
        }
}
void GPIO_WriteToOutputPort(GPIO_RegDef_t *pGPIOx, uint16_t Value){
    pGPIOx -> ODR = Value;

}
void GPIO_ToggleOutputPin(GPIO_RegDef_t *pGPIOx, uint8_t PinNumber){
    pGPIOx ->ODR ^= (1 << PinNumber);
}
void GPIO_IRQConfig(uint8_t IRQNumber, uint8_t IRQPriority, uint8_t EnorDi){
    
    if(EnorDi == ENABLE){
        if(IRQNumber <= 31){
            //program ISER0 register
            *NVIC_ISER0 |= (1 << IRQNumber);
        }else if(IRQNumber > 31 && IRQNumber < 64){
            //program ISER1 register
            *NVIC_ISER1 |= (1 << (IRQNumber % 32));
        }else if(IRQNumber >= 64 && IRQNumber < 96){
            //program ISER2 register
            *NVIC_ISER2 |= (1 << (IRQNumber % 64));
        }
    }else{
        if(IRQNumber <= 31){
            //program ICER0 register
            *NVIC_ICER0 |= (1 << IRQNumber);
        }else if(IRQNumber > 31 && IRQNumber < 64){
            //program ICER1 register
            *NVIC_ICER1 |= (1 << (IRQNumber % 32));
        }else if(IRQNumber >= 64 && IRQNumber < 96){
            //program ICER2 register
            *NVIC_ICER2 |= (1 << (IRQNumber % 64));
        }
    }
}
void GPIO_IRQPriorityConfig(uint8_t IRQNumber, uint32_t IRQPriority){
    //1. first lets find out the ipr register
    uint8_t iprx = IRQNumber / 4;
    uint8_t iprx_section = IRQNumber % 4;
    uint8_t shift_amount = (8 * iprx_section) + (8 - NO_PR_BITS_IMPLEMENTED);
    *(NVIC_BASEADDR + iprx) |= (IRQPriority << shift_amount);
}
void GPIO_IRQHandling(uint8_t PinNumber){
    if(EXIT ->PR1 & (1 << PinNumber)){
        //clear the pending register
        EXIT ->PR1 |= (1 << PinNumber);
    }
};
