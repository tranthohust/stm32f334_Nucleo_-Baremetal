#include "STM32F334_CORE.h"
#include "board_peripherals.h"
#include "communication_demo.h"
#include "demo_config.h"
#if FIRMWARE_USE_FREERTOS
#include "rtos_demo.h"
#endif

int main(void)
{
    /* 1. Timebase truoc: cac driver dung millis cho timeout. */
    if (!Timebase_Init(DEMO_CORE_HZ)) {
        Board_LEDInit();
        GPIO_WriteToOutputPin(GPIOA, GPIO_PIN_NO_5, GPIO_PIN_SET);
        for (;;) { }
    }

#if FIRMWARE_USE_FREERTOS
    /* RTOS task entry se Init driver; main tao task va bat scheduler. */
    RTOS_Start();
#else
    if (!CommunicationDemo_Init()) {
        Board_LEDInit();
        GPIO_WriteToOutputPin(GPIOA, GPIO_PIN_NO_5, GPIO_PIN_SET);
        for (;;) { }
    }

    /* 2. ISR di chuyen du lieu. Main xu ly protocol va callback.
     * Doc theo thu tu: main -> app -> line_parser -> UART -> DMA ISR. */
    while (1)
        CommunicationDemo_Process();
#endif
}
