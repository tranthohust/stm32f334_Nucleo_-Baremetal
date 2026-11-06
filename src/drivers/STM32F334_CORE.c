#include "STM32F334_CORE.h"
#if FIRMWARE_USE_FREERTOS
#include "FreeRTOS.h"
#include "task.h"
void xPortSysTickHandler(void);
_Static_assert(configTICK_RATE_HZ == 1000U, "Timebase needs a 1ms RTOS tick");
#endif

#define CORE_REG32(address) (*(volatile uint32_t *)(address))
#define SYSTICK_CTRL CORE_REG32(0xE000E010U)
#define SYSTICK_LOAD CORE_REG32(0xE000E014U)
#define SYSTICK_VAL  CORE_REG32(0xE000E018U)
#define NVIC_PRIORITY_BITS 4U

static volatile uint32_t milliseconds;

uint32_t IRQ_Save(void)
{
    uint32_t saved;
    /* Luu trang thai cu de critical section long nhau van khoi phuc dung.
     * 'memory' ngan compiler day truy cap buffer qua bien critical section. */
    __asm volatile ("mrs %0, primask\n\tcpsid i" : "=r"(saved) :: "memory");
    return saved;
}

void IRQ_Restore(uint32_t saved_primask)
{
    __asm volatile ("msr primask, %0" :: "r"(saved_primask) : "memory");
}

void NVIC_EnableIRQ(uint8_t irq)
{
    if (irq < 96U)
        ((volatile uint32_t *)0xE000E100U)[irq / 32U] = 1UL << (irq % 32U);
}

void NVIC_DisableIRQ(uint8_t irq)
{
    /* ICER la write-one-to-clear: khong doc/sua/ghi voi |=. */
    if (irq < 96U)
        ((volatile uint32_t *)0xE000E180U)[irq / 32U] = 1UL << (irq % 32U);
    __asm volatile ("dsb\n\tisb" ::: "memory");
}

void NVIC_ClearPendingIRQ(uint8_t irq)
{
    if (irq < 96U)
        ((volatile uint32_t *)0xE000E280U)[irq / 32U] = 1UL << (irq % 32U);
}

void NVIC_SetPriority(uint8_t irq, uint8_t priority)
{
    /* IPR bat dau 0xE000E400, khac ISER (0xE000E100).
     * Moi IRQ chiem 1 byte; ghi de byte de co the tang HOAC giam priority. */
    if (irq < 96U && priority < (1U << NVIC_PRIORITY_BITS))
        ((volatile uint8_t *)0xE000E400U)[irq] =
            (uint8_t)(priority << (8U - NVIC_PRIORITY_BITS));
}

bool Timebase_Init(uint32_t core_hz)
{
    uint32_t ticks = core_hz / 1000U;
    if (ticks == 0U || ticks > 0x1000000U || core_hz % 1000U != 0U)
        return false;

    SYSTICK_CTRL = 0U;
    milliseconds = 0U;
    SYSTICK_LOAD = ticks - 1U;
    SYSTICK_VAL = 0U;
    /* SysTick priority thap hon IRQ du lieu: khong lam tre RX collect. */
    *(volatile uint8_t *)0xE000ED23U = 0xF0U;
    SYSTICK_CTRL = 7U; /* CLKSOURCE=CPU, TICKINT=1, ENABLE=1. */
    return true;
}

uint32_t Timebase_Millis(void)
{
    return milliseconds;
}

void SysTick_Handler(void)
{
    ++milliseconds;
#if FIRMWARE_USE_FREERTOS
    /* Timebase chay ca truoc scheduler de Init co timeout. Port tu cau hinh
     * lai SysTick khi StartScheduler, cung tick1ms. SUSPENDED van phai tick
     * de kernel ghi nhan pended ticks; chi bo qua NOT_STARTED. */
    if (xTaskGetSchedulerState() != taskSCHEDULER_NOT_STARTED)
        xPortSysTickHandler();
#endif
}

