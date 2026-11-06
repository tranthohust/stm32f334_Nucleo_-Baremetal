#include "STM32F334_GPIO.h"

int main(void)
{
    GPIO_Handle_t led = {0};
    led.pGPIOx = GPIOA;
    led.GPIO_PinConfig.GPIO_PinNumber = GPIO_PIN_NO_5;
    led.GPIO_PinConfig.GPIO_PinMode = GPIO_MODE_OUT;
    led.GPIO_PinConfig.GPIO_PinSpeed = GPIO_SPEED_LOW;
    led.GPIO_PinConfig.GPIO_PinOPType = GPIO_OP_TYPE_PP;
    led.GPIO_PinConfig.GPIO_PinPuPdControl = GPIO_NO_PUPD;

    GPIO_PeriClockControl(GPIOA, ENABLE);
    GPIO_Init(&led);

    while (1)
    {
        GPIO_ToggleOutputPin(GPIOA, GPIO_PIN_NO_5);
        /* Approximate visual delay only; not a calibrated time base. */
        for (volatile uint32_t i = 0; i < 400000U; ++i)
            __asm volatile ("nop");
    }
}
