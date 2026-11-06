#include "board_peripherals.h"
#include "demo_config.h"

/* Giong GPIO_Handle_t: con tro thanh ghi + cau hinh + trang thai runtime.
 * Global khong initializer nam trong .bss; Init ben duoi gan config ro rang.
 * Khong copy handle hay sua cau hinh khi driver dang hoat dong. */
UART_Handle_t huart2;
SPI_Handle_t hspi1;
CAN_Handle_t hcan1;
I2C_Handle_t hi2c1;
TIM_Handle_t htim3, hpwm2;
ADC_Handle_t hadc1;

/* Pin mapping thuoc board. Driver UART/SPI/CAN khong tu chon PA/PB.
 * Helper nay van goi GPIO_Init theo dung form ban da viet. */
static void alternate_pin(GPIO_RegDef_t *port, uint8_t pin, uint8_t af,
                          uint8_t pull)
{
    GPIO_Handle_t gpio = {0};
    gpio.pGPIOx = port;
    gpio.GPIO_PinConfig.GPIO_PinNumber = pin;
    gpio.GPIO_PinConfig.GPIO_PinMode = GPIO_MODE_ALTFN;
    gpio.GPIO_PinConfig.GPIO_PinSpeed = GPIO_SPEED_MEDIUM;
    gpio.GPIO_PinConfig.GPIO_PinPuPdControl = pull;
    gpio.GPIO_PinConfig.GPIO_PinOPType = GPIO_OP_TYPE_PP;
    gpio.GPIO_PinConfig.GPIO_PinAltFunMode = af;
    GPIO_PeriClockControl(port, ENABLE);
    (void)RCC->AHBENR;
    GPIO_Init(&gpio);
}

static void output_pin(GPIO_RegDef_t *port, uint8_t pin, uint8_t initial)
{
    GPIO_Handle_t gpio = {0};
    gpio.pGPIOx = port;
    gpio.GPIO_PinConfig.GPIO_PinNumber = pin;
    gpio.GPIO_PinConfig.GPIO_PinMode = GPIO_MODE_OUT;
    gpio.GPIO_PinConfig.GPIO_PinSpeed = GPIO_SPEED_MEDIUM;
    gpio.GPIO_PinConfig.GPIO_PinPuPdControl = GPIO_NO_PUPD;
    gpio.GPIO_PinConfig.GPIO_PinOPType = GPIO_OP_TYPE_PP;
    GPIO_PeriClockControl(port, ENABLE);
    (void)RCC->AHBENR;
    /* Dat latch truoc khi chuyen MODER de tranh xung CS thap ngoai y muon. */
    GPIO_WriteToOutputPin(port, pin, initial);
    GPIO_Init(&gpio);
}

void Board_LEDInit(void)
{
    output_pin(GPIOA, GPIO_PIN_NO_5, GPIO_PIN_RESET);
}

bool Board_UARTInit(void)
{
    if (huart2.Initialized) return false;
    huart2.pUSARTx = USART2;
    huart2.UART_Config.UART_PCLKHz = DEMO_APB1_HZ;
    huart2.UART_Config.UART_BaudRate = DEMO_UART_BAUD;
    huart2.UART_Config.UART_RxMode = DEMO_UART_USE_DMA ? UART_RX_DMA : UART_RX_IRQ;
    huart2.UART_Config.UART_IRQPriority = 5U;
    alternate_pin(GPIOA, GPIO_PIN_NO_2, GPIO_AF7, GPIO_NO_PUPD);
    alternate_pin(GPIOA, GPIO_PIN_NO_3, GPIO_AF7, GPIO_PIN_PU);
    return UART_Init(&huart2); /* Init bat clock UART va DMA can thiet. */
}

bool Board_SPIInit(void)
{
    if (hspi1.Initialized) return false;
    hspi1.pSPIx = SPI1;
    hspi1.SPI_Config.SPI_DeviceMode = SPI_DEVICE_MODE_MASTER;
    hspi1.SPI_Config.SPI_DataFrameSize = SPI_DFF_8BITS;
    hspi1.SPI_Config.SPI_CPOL = SPI_CPOL_LOW;
    hspi1.SPI_Config.SPI_CPHA = SPI_CPHA_FIRST_EDGE;
    hspi1.SPI_Config.SPI_SclkSpeed = SPI_SCLK_SPEED_DIV16;
    hspi1.SPI_Config.SPI_IRQPriority = 3U;
    alternate_pin(GPIOB, GPIO_PIN_NO_3, GPIO_AF5, GPIO_NO_PUPD);
    alternate_pin(GPIOB, GPIO_PIN_NO_4, GPIO_AF5, GPIO_NO_PUPD);
    alternate_pin(GPIOB, GPIO_PIN_NO_5, GPIO_AF5, GPIO_NO_PUPD);
    output_pin(GPIOB, GPIO_PIN_NO_6, GPIO_PIN_SET);
    return SPI_Init(&hspi1);
}

bool Board_CANInit(void)
{
    if (hcan1.initialized) return false;
    hcan1.pCANx = CAN1;
    hcan1.CAN_Config.CAN_PCLKHz = DEMO_APB1_HZ;
    hcan1.CAN_Config.CAN_BitRate = DEMO_CAN_BITRATE;
    hcan1.CAN_Config.CAN_Mode = DEMO_CAN_LOOPBACK ? CAN_MODE_SILENT_LOOPBACK : CAN_MODE_NORMAL;
    hcan1.CAN_Config.CAN_IRQPriority = 6U;
    hcan1.CAN_Config.CAN_TxTimeoutMs = 100U;
    alternate_pin(GPIOA, GPIO_PIN_NO_11, GPIO_AF9, GPIO_PIN_PU);
    alternate_pin(GPIOA, GPIO_PIN_NO_12, GPIO_AF9, GPIO_NO_PUPD);
    return CAN_Init(&hcan1);
}

bool Board_I2CInit(void)
{
    if (hi2c1.Initialized) return false;
    hi2c1.pI2Cx = I2C1;
    hi2c1.I2C_Config.I2C_KernelHz = DEMO_I2C_KERNEL_HZ;
    hi2c1.I2C_Config.I2C_Timing = I2C_TIMING_100KHZ_HSI8;
    hi2c1.I2C_Config.I2C_IRQPriority = 6U; /* Duoc phep goi ...FromISR. */

    /* PB8=SCL, PB9=SDA AF4: open-drain, khac SPI push-pull.
     * Can pull-up NGOAI len 3.3V (vi du 4.7k, can tinh theo bus thuc te).
     * Khong dung PB6/PB7 vi PB6 dang la CS cua SPI1. */
    GPIO_Handle_t pin = {0};
    pin.pGPIOx = GPIOB;
    pin.GPIO_PinConfig.GPIO_PinMode = GPIO_MODE_ALTFN;
    pin.GPIO_PinConfig.GPIO_PinSpeed = GPIO_SPEED_MEDIUM;
    pin.GPIO_PinConfig.GPIO_PinPuPdControl = GPIO_NO_PUPD;
    pin.GPIO_PinConfig.GPIO_PinOPType = GPIO_OP_TYPE_OD;
    pin.GPIO_PinConfig.GPIO_PinAltFunMode = GPIO_AF4;
    GPIO_PeriClockControl(GPIOB, ENABLE);
    (void)RCC->AHBENR;
    pin.GPIO_PinConfig.GPIO_PinNumber = GPIO_PIN_NO_8;
    GPIO_Init(&pin);
    pin.GPIO_PinConfig.GPIO_PinNumber = GPIO_PIN_NO_9;
    GPIO_Init(&pin);
    return I2C_Init(&hi2c1);
}

bool Board_TimerInit(void)
{
    if (htim3.Initialized) return false;
    htim3.pTIMx = TIM3;
    htim3.TIM_Config.TIM_ClockHz = DEMO_TIMER_HZ;
    htim3.TIM_Config.TIM_Prescaler = 7999U;
    htim3.TIM_Config.TIM_Period = 99U; /* 8MHz /8000 /100 = 10Hz =100ms. */
    htim3.TIM_Config.TIM_Mode = TIM_MODE_BASE_IRQ;
    htim3.TIM_Config.TIM_IRQPriority = 6U;
    return TIM_Init(&htim3);
}

bool Board_PWMInit(void)
{
    if (hpwm2.Initialized) return false;
    hpwm2.pTIMx = TIM2;
    hpwm2.TIM_Config.TIM_ClockHz = DEMO_TIMER_HZ;
    hpwm2.TIM_Config.TIM_Prescaler = 7U;
    hpwm2.TIM_Config.TIM_Period = 999U; /* 8MHz /8 /1000 = 1kHz. */
    hpwm2.TIM_Config.TIM_Mode = TIM_MODE_PWM_CH1;
    hpwm2.TIM_Config.TIM_IRQPriority = 6U; /* PWM nay khong can IRQ. */
    alternate_pin(GPIOA, GPIO_PIN_NO_0, GPIO_AF1, GPIO_PIN_PD);
    return TIM_Init(&hpwm2);
}

bool Board_ADCInit(void)
{
    if (hadc1.Initialized || hadc1.ProcessRunning) return false;
    hadc1.pADCx = ADC1;
    hadc1.ADC_Config.ADC_CoreClockHz = DEMO_CORE_HZ;
    hadc1.ADC_Config.ADC_Channel = 2U; /* PA1 = ADC1_IN2; khong phai channel1. */
    hadc1.ADC_Config.ADC_SampleTime = ADC_SAMPLE_601CYCLES_5;
    hadc1.ADC_Config.ADC_IRQPriority = 6U;
    GPIO_Handle_t pin = {0};
    pin.pGPIOx = GPIOA;
    pin.GPIO_PinConfig.GPIO_PinNumber = GPIO_PIN_NO_1;
    pin.GPIO_PinConfig.GPIO_PinMode = GPIO_MODE_ANALOG;
    pin.GPIO_PinConfig.GPIO_PinPuPdControl = GPIO_NO_PUPD;
    GPIO_PeriClockControl(GPIOA, ENABLE);
    (void)RCC->AHBENR;
    GPIO_Init(&pin);
    return ADC_Init(&hadc1);
}

void Board_GPIOInputInit(void)
{
    /* PB0 + nut NGOAI noi GND: released=1, pressed=0. Khong gia dinh nut B1. */
    GPIO_Handle_t pin = {0};
    pin.pGPIOx = GPIOB;
    pin.GPIO_PinConfig.GPIO_PinNumber = GPIO_PIN_NO_0;
    pin.GPIO_PinConfig.GPIO_PinMode = GPIO_MODE_IT_RFT;
    pin.GPIO_PinConfig.GPIO_PinPuPdControl = GPIO_PIN_PU;
    GPIO_PeriClockControl(GPIOB, ENABLE);
    (void)RCC->AHBENR;
    GPIO_Init(&pin);
    GPIO_IRQConfig(6U, 6U, ENABLE); /* EXTI0 route da chon PB0 trong GPIO_Init. */
}
