#include "FreeRTOS.h"
#include "task.h"
#include "rtos_hooks.h"
#include "STM32F334.h"

/* Moi doi tuong kernel dung static storage; day la bo nho rieng cua Idle.
 * StackType_t cua ARM_CM3 la 32-bit: 128 word = 512 byte, khong phai 128 byte.
 */
static StaticTask_t idle_tcb;
static StackType_t idle_stack[configMINIMAL_STACK_SIZE]
    __attribute__((aligned(8)));

volatile uint32_t g_rtos_fault_code;
volatile uint32_t g_rtos_fault_line;
const char *volatile g_rtos_fault_file;
volatile uintptr_t g_rtos_fault_task;
const char *volatile g_rtos_fault_task_name;

void vApplicationGetIdleTaskMemory(StaticTask_t **tcb,
                                   StackType_t **stack,
                                   configSTACK_DEPTH_TYPE *stack_words)
{
    *tcb = &idle_tcb;
    *stack = idle_stack;
    *stack_words = configMINIMAL_STACK_SIZE;
}

void RTOS_Fatal(uint32_t code)
{
    /* Loi co the xay ra truoc scheduler hoac trong ISR/stack bi hong.
     * Khong goi queue, mutex, driver handle, printf hay cap phat bo nho.
     */
    __asm volatile ("cpsid i" ::: "memory");
    if (g_rtos_fault_code == RTOS_FAULT_NONE) {
        g_rtos_fault_code = code == RTOS_FAULT_NONE ? RTOS_FAULT_STARTUP : code;
    }

    /* Tu setup PA5 de den van sang neu board GPIO chua duoc init. */
    RCC->AHBENR |= 1UL << 17;
    (void)RCC->AHBENR;
    GPIOA->BSRR = 1UL << 5;
    GPIOA->OTYPER &= ~(1UL << 5);
    GPIOA->PUPDR &= ~(3UL << 10);
    GPIOA->MODER = (GPIOA->MODER & ~(3UL << 10)) | (1UL << 10);
    __asm volatile ("dsb\n\tisb" ::: "memory");
    for (;;) {
        __asm volatile ("nop");
    }
}

void RTOS_AssertFailed(const char *file, uint32_t line)
{
    __asm volatile ("cpsid i" ::: "memory");
    if (g_rtos_fault_code == RTOS_FAULT_NONE) {
        g_rtos_fault_file = file;
        g_rtos_fault_line = line;
    }
    RTOS_Fatal(RTOS_FAULT_ASSERT);
}

void vApplicationStackOverflowHook(TaskHandle_t task, char *task_name)
{
    __asm volatile ("cpsid i" ::: "memory");
    if (g_rtos_fault_code == RTOS_FAULT_NONE) {
        g_rtos_fault_task = (uintptr_t)task;
        g_rtos_fault_task_name = task_name;
    }
    /* Ten/TCB co the da bi hong: chi luu pointer, khong doc chuoi ten task. */
    RTOS_Fatal(RTOS_FAULT_STACK_OVERFLOW);
}
