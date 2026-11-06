#ifndef INC_STM32F334MIN_H_
#define INC_STM32F334MIN_H_
#include <stdint.h>
#define FLASH_BASEADDR 0X08000000U
#define SRAM_BASEADDR  0x20000000U
#define ROM_BASEADDR   0x1FFF0000U
#define PERIPH_BASEADDR 0x40020000U
#define AHB1PERIPH_BASEADDR PERIPH_BASEADDR
#define AHB2PERIPH_BASEADDR 0x48000000U
#define AHB3PERIPH_BASEADDR 0x50000000U
#define APB1PERIPH_BASEADDR 0x40000000U
#define APB2PERIPH_BASEADDR 0x40010000U
//define GPIOA_BASEADDR (AHB2PERIPH_BASEADDR + 0x0000)
#define GPIOA_BASEADDR (AHB2PERIPH_BASEADDR + 0x0000)
#define GPIOB_BASEADDR (AHB2PERIPH_BASEADDR + 0x0400)
#define GPIOC_BASEADDR (AHB2PERIPH_BASEADDR + 0x0800)
#define GPIOD_BASEADDR (AHB2PERIPH_BASEADDR + 0x0C00)
#define GPIOF_BASEADDR (AHB2PERIPH_BASEADDR + 0x1400)
//define ADC1_ADC2
#define ADC1_BASEADDR (AHB3PERIPH_BASEADDR + 0x0000)
#define ADC2_BASEADDR (AHB3PERIPH_BASEADDR + 0x03FF)
//define RCC_BASEADDR (AHB1PERIPH_BASEADDR + 0x1000)
#define RCC_BASEADDR (AHB1PERIPH_BASEADDR + 0x1000)
//define I2C
#define I2C1_BASEADDR (APB1PERIPH_BASEADDR + 0x5400)
//define SPI
#define SPI1_BASEADDR (APB2PERIPH_BASEADDR + 0x3000)
//define USART
#define USART1_BASEADDR (APB2PERIPH_BASEADDR + 0x3800)
#define USART2_BASEADDR (APB1PERIPH_BASEADDR + 0x4400)
#define USART3_BASEADDR (APB1PERIPH_BASEADDR + 0x4800)
//define EXTI
#define EXTI_BASEADDR (APB2PERIPH_BASEADDR + 0x400)
//define SYSCFG
#define SYSCFG_BASEADDR (APB2PERIPH_BASEADDR + 0x0000)
#define CAN1_BASEADDR (APB1PERIPH_BASEADDR + 0x6400)

//define structure of GPIO
typedef struct
{
    volatile uint32_t MODER;
    volatile uint32_t OTYPER;
    volatile uint32_t OSPEEDR;
    volatile uint32_t PUPDR;
    volatile uint32_t IDR;
    volatile uint32_t ODR;
    volatile uint32_t BSRR;
    volatile uint32_t LCKR;
    volatile uint32_t AFR[2];
    volatile uint32_t BRR;
} GPIO_RegDef_t;
#define GPIOA ((GPIO_RegDef_t*)GPIOA_BASEADDR)
#define GPIOB ((GPIO_RegDef_t*)GPIOB_BASEADDR)
#define GPIOC ((GPIO_RegDef_t*)GPIOC_BASEADDR)
#define GPIOD ((GPIO_RegDef_t*)GPIOD_BASEADDR)
#define GPIOF ((GPIO_RegDef_t*)GPIOF_BASEADDR)

//define structure of RCC
typedef struct
{
    volatile uint32_t CR;
    volatile uint32_t CFGR;
    volatile uint32_t CIR;
    volatile uint32_t APB2RSTR;
    volatile uint32_t APB1RSTR;
    volatile uint32_t AHBENR;
    volatile uint32_t APB2ENR;
    volatile uint32_t APB1ENR;
    volatile uint32_t BDCR;
    volatile uint32_t CSR;
    volatile uint32_t AHBRSTR;
    volatile uint32_t CFGR2;
    volatile uint32_t CFGR3;
}RCC_RegDef_t;
#define RCC ((RCC_RegDef_t*)RCC_BASEADDR)
 //define structure of EXTI
 typedef struct
 {
     volatile uint32_t IMR1;
     volatile uint32_t EMR1;
     volatile uint32_t RTSR1;
     volatile uint32_t FTSR1;
     volatile uint32_t SWIER1;
     volatile uint32_t PR1;
     volatile uint32_t IMR2;
     volatile uint32_t EMR2;       
     volatile uint32_t RTSR2;
     volatile uint32_t FTSR2;
     volatile uint32_t SWIER2;
     volatile uint32_t PR2;
 }EXIT_RegDef_t;
 #define EXIT ((EXIT_RegDef_t *)EXTI_BASEADDR)
 //define structure of spi
 typedef struct
 {
    volatile uint32_t CR1;
    volatile uint32_t CR2;
    volatile uint32_t SR;
    volatile uint32_t DR;
    volatile uint32_t CRCPR;
    volatile uint32_t RXCRCR;
    volatile uint32_t TXCRCR;

 }SPI_RegDef_t;
 #define SPI1 ((SPI_RegDef_t*)SPI1_BASEADDR)
 //define structure of i2c
 typedef struct
 {
    volatile uint32_t CR1;
    volatile uint32_t CR2;
    volatile uint32_t OAR1;
    volatile uint32_t OAR2;
    volatile uint32_t TIMINGR;
    volatile uint32_t TIMEOUTR;
    volatile uint32_t ISR;
    volatile uint32_t ICR;
    volatile uint32_t PECR;
    volatile uint32_t RXDR;
    volatile uint32_t TXDR;
 }I2C_RegDef_t;
 #define I2C1 ((I2C_RegDef_t*)I2C1_BASEADDR)
//define structure of usart
 typedef struct
 {
    volatile uint32_t CR1;
    volatile uint32_t CR2;
    volatile uint32_t CR3;
    volatile uint32_t BRR;
    volatile uint32_t GTPR;
    volatile uint32_t RTOR;
    volatile uint32_t RQR;
    volatile uint32_t ISR;
    volatile uint32_t ICR;
    volatile uint32_t RDR;
    volatile uint32_t TDR;

 }USART_RegDef_t;
 #define USART1 ((USART_RegDef_t*)USART1_BASEADDR)
 #define USART2 ((USART_RegDef_t*)USART2_BASEADDR)
 #define USART3 ((USART_RegDef_t*)USART3_BASEADDR)
 //define structure of SYSCFG
    typedef struct
    {
        volatile uint32_t CFGR1;
        volatile uint32_t RCR;
        volatile uint32_t EXTICR[4];
        volatile uint32_t CFGR2;
        volatile uint32_t CFGR3;
    }SYSCFG_RegDef_t;
    #define SYSCFG ((SYSCFG_RegDef_t*)SYSCFG_BASEADDR)
//define structure of ADC
    typedef struct
    {
        volatile uint32_t ISR;
        volatile uint32_t IER;
        volatile uint32_t CR;
        volatile uint32_t CFGR;
        volatile uint32_t SMPR1;
        volatile uint32_t SMPR2;
        volatile uint32_t TR1;
        volatile uint32_t TR2;
        volatile uint32_t TR3;
        volatile uint32_t SQR1;
        volatile uint32_t SQR2;
        volatile uint32_t SQR3;
        volatile uint32_t SQR4;
        volatile uint32_t DR;
        volatile uint32_t JSQR;
        volatile uint32_t OFR1;
        volatile uint32_t OFR2;
        volatile uint32_t OFR3;
        volatile uint32_t OFR4;
        volatile uint32_t JDR1;
        volatile uint32_t JDR2;
        volatile uint32_t JDR3;
        volatile uint32_t JDR4;
        volatile uint32_t AWD2CR;
        volatile uint32_t AWD3CR;
        volatile uint32_t DIFSEL;
        volatile uint32_t CALFACT;
        volatile uint32_t CCR;
        volatile uint32_t CCR;
        volatile uint32_t CDR;
    }ADC_RegDef_t;
    #define ADC1 ((ADC_RegDef_t*)ADC1_BASEADDR)
    #define ADC2 ((ADC_RegDef_t*)ADC2_BASEADDR)
//define structure of CAN

typedef struct
    {
        volatile uint32_t MCR;
        volatile uint32_t MSR;
        volatile uint32_t TSR;
        volatile uint32_t RF0R;
        volatile uint32_t RF1R;
        volatile uint32_t IER;
        volatile uint32_t ESR;
        volatile uint32_t BTR;
        volatile uint32_t RESERVED0[88];
        volatile uint32_t TI0R;
        volatile uint32_t TDT0R;
        volatile uint32_t TDL0R;
        volatile uint32_t TDH0R;
        volatile uint32_t TI1R;
        volatile uint32_t TDT1R;
        volatile uint32_t TDL1R;
        volatile uint32_t TDH1R;
        volatile uint32_t TI2R;
        volatile uint32_t TDT2R;
        volatile uint32_t TDL2R;
        volatile uint32_t TDH2R;
    }CAN_RegDef_t;
    #define CAN1 ((CAN_RegDef_t*)CAN1_BASEADDR)
//enable clock for gpio
#define GPIOA_PCLK_EN() (RCC->AHBENR |= (1<<17))
#define GPIOB_PCLK_EN() (RCC->AHBENR |= (1<<18))
#define GPIOC_PCLK_EN() (RCC->AHBENR |= (1<<19))
#define GPIOD_PCLK_EN() (RCC->AHBENR |= (1<<20))
#define GPIOF_PCLK_EN() (RCC->AHBENR |= (1<<22))
//disable clock for gpio
#define GPIOA_PCLK_DI() (RCC->AHBENR &= ~(1<<17))
#define GPIOB_PCLK_DI() (RCC->AHBENR &= ~(1<<18))
#define GPIOC_PCLK_DI() (RCC->AHBENR &= ~(1<<19))
#define GPIOD_PCLK_DI() (RCC->AHBENR &= ~(1<<20))
#define GPIOF_PCLK_DI() (RCC->AHBENR &= ~(1<<22))
//gpio pin reset
#define GPIOA_REG_RESET() do{(RCC->AHBRSTR |= (1<<17)); (RCC->AHBRSTR &= ~(1<<17));}while(0)
#define GPIOB_REG_RESET() do{(RCC->AHBRSTR |= (1<<18)); (RCC->AHBRSTR &= ~(1<<18));}while(0)
#define GPIOC_REG_RESET() do{(RCC->AHBRSTR |= (1<<19)); (RCC->AHBRSTR &= ~(1<<19));}while(0)
#define GPIOD_REG_RESET() do{(RCC->AHBRSTR |= (1<<20)); (RCC->AHBRSTR &= ~(1<<20));}while(0)
#define GPIOF_REG_RESET() do{(RCC->AHBRSTR |= (1<<22)); (RCC->AHBRSTR &= ~(1<<22));}while(0)

#define GPIO_BASEADDR_TO_CODE(x)          \
    (((x) == GPIOA) ? 0U :               \
     ((x) == GPIOB) ? 1U :               \
     ((x) == GPIOC) ? 2U :               \
     ((x) == GPIOD) ? 3U :               \
     ((x) == GPIOF) ? 5U : 0U)
//enable clock for i2c
#define I2C1_PCLK_EN() (RCC->APB1ENR |= (1<<21))
//disable clock for i2c
#define I2C1_PCLK_DI() (RCC->APB1ENR &= ~(1<<21))

//enable clock for spi
#define SPI1_PCLK_EN() (RCC->APB2ENR |= (1<<12))
//disable clock for spi
#define SPI1_PCLK_DI() (RCC->APB2ENR &= ~(1<<12))

//enable clock for usart
#define USART1_PCLK_EN() (RCC->APB2ENR |= (1<<14))
#define USART2_PCLK_EN() (RCC->APB1ENR |= (1<<17))
#define USART3_PCLK_EN() (RCC->APB1ENR |= (1<<18))
//disable clock for usart
#define USART1_PCLK_DI() (RCC->APB2ENR &= ~(1<<14))
#define USART2_PCLK_DI() (RCC->APB1ENR &= ~(1<<17))
#define USART3_PCLK_DI() (RCC->APB1ENR &= ~(1<<18))

//enable clock for syscfg
#define SYSCFG_PCLK_EN() (RCC->APB2ENR |= (1<<0))
//disable clock for syscfgs
#define SYSCFG_PCLK_DI() (RCC->APB2ENR &= ~(1<<0))
//define GPIO resset
#define ENABLE 1
#define DISABLE 0
#define SET ENABLE
#define RESET DISABLE
#define GPIO_PIN_SET SET
#define GPIO_PIN_RESET RESET
#endif /* INC_STM32F334MIN_H_ */