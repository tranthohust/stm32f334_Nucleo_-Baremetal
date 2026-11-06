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
#define ADC2_BASEADDR (AHB3PERIPH_BASEADDR + 0x0100)
#define ADC12_COMMON_BASEADDR (AHB3PERIPH_BASEADDR + 0x0300)
#define TIM2_BASEADDR (APB1PERIPH_BASEADDR + 0x0000)
#define TIM3_BASEADDR (APB1PERIPH_BASEADDR + 0x0400)
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
//define nvic register structure
#define NVIC_ISER0 ((volatile uint32_t*)0xE000E100)
#define NVIC_ISER1 ((volatile uint32_t*)0xE000E104)
#define NVIC_ISER2 ((volatile uint32_t*)0xE000E108)
#define NVIC_ICER0 ((volatile uint32_t*)0xE000E180)
#define NVIC_ICER1 ((volatile uint32_t*)0xE000E184)
#define NVIC_ICER2 ((volatile uint32_t*)0xE000E188)
#define NVIC_BASEADDR ((volatile uint32_t *)0xE000E100)
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
    uint32_t RESERVED0;
        volatile uint32_t SMPR1;
        volatile uint32_t SMPR2;
    uint32_t RESERVED1;
        volatile uint32_t TR1;
        volatile uint32_t TR2;
        volatile uint32_t TR3;
    uint32_t RESERVED2;
        volatile uint32_t SQR1;
        volatile uint32_t SQR2;
        volatile uint32_t SQR3;
        volatile uint32_t SQR4;
        volatile uint32_t DR;
    uint32_t RESERVED3[2];
        volatile uint32_t JSQR;
    uint32_t RESERVED4[4];
        volatile uint32_t OFR1;
        volatile uint32_t OFR2;
        volatile uint32_t OFR3;
        volatile uint32_t OFR4;
    uint32_t RESERVED5[4];
        volatile uint32_t JDR1;
        volatile uint32_t JDR2;
        volatile uint32_t JDR3;
        volatile uint32_t JDR4;
    uint32_t RESERVED6[4];
        volatile uint32_t AWD2CR;
        volatile uint32_t AWD3CR;
    uint32_t RESERVED7[2];
        volatile uint32_t DIFSEL;
        volatile uint32_t CALFACT;
    }ADC_RegDef_t;
    #define ADC1 ((ADC_RegDef_t*)ADC1_BASEADDR)
    #define ADC2 ((ADC_RegDef_t*)ADC2_BASEADDR)
typedef struct {
    volatile uint32_t CSR;
    uint32_t RESERVED;
    volatile uint32_t CCR;
    volatile uint32_t CDR;
} ADC_Common_RegDef_t;
#define ADC12_COMMON ((ADC_Common_RegDef_t *)ADC12_COMMON_BASEADDR)

/* TIM2/TIM3 layout: TIM3 has reserved RCR/BDTR slots; never write them. */
typedef struct {
    volatile uint32_t CR1, CR2, SMCR, DIER, SR, EGR;
    volatile uint32_t CCMR1, CCMR2, CCER, CNT, PSC, ARR;
    uint32_t RESERVED_RCR;
    volatile uint32_t CCR1, CCR2, CCR3, CCR4;
    uint32_t RESERVED_BDTR;
    volatile uint32_t DCR, DMAR;
} TIM_RegDef_t;
#define TIM2 ((TIM_RegDef_t *)TIM2_BASEADDR)
#define TIM3 ((TIM_RegDef_t *)TIM3_BASEADDR)
//define structure of CAN

/* Register map dung chung cho handle CAN, giong GPIO/SPI/USART.
 * TX/RX mailbox 16 byte; filter ID/MASK 8 byte. Offset theo RM0364. */
typedef struct
{
    volatile uint32_t ID;
    volatile uint32_t LENGTH;
    volatile uint32_t LOW;
    volatile uint32_t HIGH;
} CAN_Mailbox_RegDef_t;

typedef struct
{
    volatile uint32_t ID;
    volatile uint32_t MASK;
} CAN_Filter_RegDef_t;

typedef struct
{
    volatile uint32_t MCR, MSR, TSR, RF0R, RF1R, IER, ESR, BTR;
    uint32_t RESERVED0[88];
    CAN_Mailbox_RegDef_t TX[3];        /* 0x180 */
    CAN_Mailbox_RegDef_t RX[2];        /* 0x1B0 */
    uint32_t RESERVED1[12];
    volatile uint32_t FMR, FM1R;       /* 0x200 */
    uint32_t RESERVED2;
    volatile uint32_t FS1R;
    uint32_t RESERVED3;
    volatile uint32_t FFA1R;
    uint32_t RESERVED4;
    volatile uint32_t FA1R;
    uint32_t RESERVED5[8];
    CAN_Filter_RegDef_t FILTER[14];    /* 0x240 */
} CAN_RegDef_t;
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
#define I2C1_REG_RESET() do { RCC->APB1RSTR |= (1UL << 21); RCC->APB1RSTR &= ~(1UL << 21); } while (0)

//enable clock for spi
#define SPI1_PCLK_EN() (RCC->APB2ENR |= (1<<12))
//disable clock for spi
#define SPI1_PCLK_DI() (RCC->APB2ENR &= ~(1<<12))
#define SPI1_REG_RESET() do { RCC->APB2RSTR |= (1UL << 12); RCC->APB2RSTR &= ~(1UL << 12); } while (0)

//enable clock for usart
#define USART1_PCLK_EN() (RCC->APB2ENR |= (1<<14))
#define USART2_PCLK_EN() (RCC->APB1ENR |= (1<<17))
#define USART3_PCLK_EN() (RCC->APB1ENR |= (1<<18))
//disable clock for usart
#define USART1_PCLK_DI() (RCC->APB2ENR &= ~(1<<14))
#define USART2_PCLK_DI() (RCC->APB1ENR &= ~(1<<17))
#define USART3_PCLK_DI() (RCC->APB1ENR &= ~(1<<18))
#define USART2_REG_RESET() do { RCC->APB1RSTR |= (1UL << 17); RCC->APB1RSTR &= ~(1UL << 17); } while (0)

//enable/disable clock and reset for CAN1
#define CAN1_PCLK_EN() (RCC->APB1ENR |= (1UL << 25))
#define CAN1_PCLK_DI() (RCC->APB1ENR &= ~(1UL << 25))
#define CAN1_REG_RESET() do { RCC->APB1RSTR |= (1UL << 25); RCC->APB1RSTR &= ~(1UL << 25); } while (0)

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
