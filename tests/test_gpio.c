/* Native tests execute the actual GPIO driver with RAM-backed registers. */
#include "../src/drivers/STM32F334_GPIO.h"
#include "../src/drivers/STM32F334_CORE.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

static GPIO_RegDef_t port_a, port_b;
static RCC_RegDef_t clocks;
static EXIT_RegDef_t exti;
static SYSCFG_RegDef_t syscfg;
static uint8_t last_irq, last_priority;
static unsigned enables, disables, priorities;

#undef GPIOA
#undef GPIOB
#undef RCC
#undef EXIT
#undef SYSCFG
#define GPIOA (&port_a)
#define GPIOB (&port_b)
#define RCC (&clocks)
#define EXIT (&exti)
#define SYSCFG (&syscfg)

void NVIC_EnableIRQ(uint8_t irq) { last_irq = irq; ++enables; }
void NVIC_DisableIRQ(uint8_t irq) { last_irq = irq; ++disables; }
void NVIC_SetPriority(uint8_t irq, uint8_t priority)
{
    last_irq = irq;
    last_priority = priority;
    ++priorities;
}

#include "../src/drivers/STM32F334.c"

int main(void)
{
    /* Atomic per-pin write: ODR must not be read/rewritten by this function.
     * A pending update of another pin cannot be overwritten by an old ODR. */
    port_a.ODR = 0xA55AU;
    GPIO_WriteToOutputPin(GPIOA, 5U, GPIO_PIN_SET);
    assert(port_a.BSRR == (1UL << 5U) && port_a.ODR == 0xA55AU);
    GPIO_WriteToOutputPin(GPIOA, 15U, GPIO_PIN_RESET);
    assert(port_a.BSRR == (1UL << 31U) && port_a.ODR == 0xA55AU);
    GPIO_WriteToOutputPin(GPIOA, 16U, GPIO_PIN_SET);
    assert(port_a.BSRR == (1UL << 31U));
    GPIO_WriteToOutputPin(NULL, 0U, GPIO_PIN_SET);

    /* Upper-bit AF configuration and unrelated AF/MODER fields survive. */
    GPIO_Handle_t pin = {0};
    pin.pGPIOx = GPIOB;
    pin.GPIO_PinConfig.GPIO_PinNumber = 15U;
    pin.GPIO_PinConfig.GPIO_PinMode = GPIO_MODE_ALTFN;
    pin.GPIO_PinConfig.GPIO_PinSpeed = GPIO_SPEED_MEDIUM;
    pin.GPIO_PinConfig.GPIO_PinAltFunMode = GPIO_AF9;
    port_b.AFR[1] = 0x12345678U;
    port_b.MODER = 0xFFFFFFFFU;
    GPIO_Init(&pin);
    assert(port_b.AFR[1] == 0x92345678U);
    assert((port_b.MODER >> 30U) == 2U);
    assert((port_b.MODER & 0x3FFFFFFFU) == 0x3FFFFFFFU);

    /* EXTI selects input, routes correct port, unmasks IRQ, clears own flag. */
    pin.GPIO_PinConfig.GPIO_PinNumber = 13U;
    pin.GPIO_PinConfig.GPIO_PinMode = GPIO_MODE_IT_RT;
    exti.FTSR1 = 0xFFFFU;
    syscfg.EXTICR[3] = 0xFFFFU;
    GPIO_Init(&pin);
    assert((port_b.MODER & (3UL << 26U)) == 0U);
    assert((exti.RTSR1 & (1UL << 13U)) != 0U);
    assert((exti.FTSR1 & (1UL << 13U)) == 0U);
    assert((exti.IMR1 & (1UL << 13U)) != 0U);
    assert(syscfg.EXTICR[3] == 0xFF1FU);
    assert(exti.PR1 == (1UL << 13U));
    exti.PR1 = (1UL << 13U) | (1UL << 7U);
    GPIO_IRQHandling(13U);
    assert(exti.PR1 == (1UL << 13U)); /* W1C write mask, not OR of pending flags. */

    GPIO_IRQConfig(35U, 7U, ENABLE);
    assert(enables == 1U && priorities == 1U && last_irq == 35U && last_priority == 7U);
    GPIO_IRQConfig(35U, 7U, DISABLE);
    assert(disables == 1U && priorities == 1U);
    GPIO_IRQConfig(96U, 0U, ENABLE);
    GPIO_IRQConfig(1U, 16U, ENABLE);
    assert(enables == 1U && priorities == 1U);
    puts("GPIO register-mock tests passed");
    return 0;
}
