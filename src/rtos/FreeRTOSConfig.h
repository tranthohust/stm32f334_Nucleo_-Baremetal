#ifndef FREERTOS_CONFIG_H
#define FREERTOS_CONFIG_H

#include <stdint.h>
#include "rtos_hooks.h"

/* FreeRTOS-Kernel V11.3.3, portable/GCC/ARM_CM3.
 * STM32F334 Cortex-M4 chay tap lenh ARMv7-M nay khi build -mfloat-abi=soft.
 * Khong dung FPU trong task/ISR: port nay khong save/restore thanh ghi FPU.
 * HSI reset giu CPU va APB o 8MHz; neu doi clock phai doi cau hinh nay.
 */
#define configCPU_CLOCK_HZ                       8000000UL
#define configTICK_RATE_HZ                       1000U
#define configUSE_PREEMPTION                     1
#define configUSE_TIME_SLICING                   1
#define configUSE_TICKLESS_IDLE                  0
#define configMAX_PRIORITIES                     4U
#define configMINIMAL_STACK_SIZE                 128U
#define configMAX_TASK_NAME_LEN                  16U
#define configTICK_TYPE_WIDTH_IN_BITS             TICK_TYPE_WIDTH_32_BITS
#define configSTACK_DEPTH_TYPE                   uint32_t
#define configIDLE_SHOULD_YIELD                   1
#define configUSE_PORT_OPTIMISED_TASK_SELECTION   1

/* StaticTask_t/StaticQueue_t va stack/storage deu do ung dung cap phat.
 * Khong build heap_1..heap_5.c, khong goi xTaskCreate hay xQueueCreate.
 * Idle TCB/stack duoc tra qua vApplicationGetIdleTaskMemory trong hooks.c.
 */
#define configSUPPORT_STATIC_ALLOCATION           1
#define configSUPPORT_DYNAMIC_ALLOCATION          0
#define configKERNEL_PROVIDED_STATIC_MEMORY       0
#define configTOTAL_HEAP_SIZE                     0U
#define configUSE_MALLOC_FAILED_HOOK              0

#define configUSE_MUTEXES                         1
#define configUSE_RECURSIVE_MUTEXES               0
#define configUSE_COUNTING_SEMAPHORES             1
#define configUSE_QUEUE_SETS                      0
#define configQUEUE_REGISTRY_SIZE                 4U
#define configUSE_TASK_NOTIFICATIONS              1
#define configTASK_NOTIFICATION_ARRAY_ENTRIES     1U
#define configUSE_TIMERS                          0
#define configUSE_CO_ROUTINES                     0

/* High-water mark do so WORD stack con trong, khong phai byte.
 * Stack overflow mode2 chi phat hien mot so tinh huong khi context switch;
 * khong thay cho do stack thuc te va khong bao ve MSP cua ISR.
 */
#define configCHECK_FOR_STACK_OVERFLOW            2
#define configUSE_TRACE_FACILITY                  1
#define configGENERATE_RUN_TIME_STATS             0
#define configUSE_STATS_FORMATTING_FUNCTIONS      0
#define configUSE_IDLE_HOOK                       0
#define configUSE_TICK_HOOK                       0
#define configUSE_DAEMON_TASK_STARTUP_HOOK         0
#define configUSE_APPLICATION_TASK_TAG            0
#define configUSE_NEWLIB_REENTRANT                0
#define configUSE_POSIX_ERRNO                     0
#define configNUM_THREAD_LOCAL_STORAGE_POINTERS   0

/* NVIC: STM32F334 co 4 bit priority; so nho la uu tien cao.
 * Priority grouping=0: 4 bit deu la preemption, khong tach subpriority.
 * IRQ0..4 duoc ngat kernel nhung TUYET DOI khong goi FreeRTOS API.
 * IRQ5..15 moi duoc goi API ...FromISR khi can, kem portYIELD_FROM_ISR.
 * Driver hien tai chi cap nhat buffer, khong goi API RTOS trong ISR.
 * Khong goi API block/yield khi dang giu IRQ_Save (PRIMASK=1).
 */
#define configPRIO_BITS                           4U
#define configLIBRARY_LOWEST_INTERRUPT_PRIORITY   15U
#define configLIBRARY_MAX_SYSCALL_INTERRUPT_PRIORITY 5U
#define configKERNEL_INTERRUPT_PRIORITY \
    (configLIBRARY_LOWEST_INTERRUPT_PRIORITY << (8U - configPRIO_BITS))
#define configMAX_SYSCALL_INTERRUPT_PRIORITY \
    (configLIBRARY_MAX_SYSCALL_INTERRUPT_PRIORITY << (8U - configPRIO_BITS))

/* SVC/PendSV tro thang vao port qua ten vector startup; khong viet C wrapper.
 * Port kiem tra hai vector nay, KHONG kiem tra SysTick.
 * SysTick_Handler van thuoc CORE.c: tang timebase va chi goi
 * xPortSysTickHandler() sau khi scheduler da bat dau. Khong macro-map SysTick.
 * Khong define configSYSTICK_CLOCK_HZ: port dung clock CPU theo mac dinh.
 */
#define vPortSVCHandler                           SVC_Handler
#define xPortPendSVHandler                        PendSV_Handler
#define configCHECK_HANDLER_INSTALLATION          1

#define INCLUDE_vTaskDelay                        1
#define INCLUDE_xTaskDelayUntil                   1
#define INCLUDE_vTaskSuspend                      1
#define INCLUDE_xTaskGetSchedulerState             1
#define INCLUDE_xTaskGetCurrentTaskHandle          1
#define INCLUDE_xTaskGetIdleTaskHandle             1
#define INCLUDE_uxTaskGetStackHighWaterMark         1
#define INCLUDE_uxTaskGetStackHighWaterMark2        1
#define INCLUDE_vTaskDelete                       0
#define INCLUDE_vTaskPrioritySet                  0
#define INCLUDE_uxTaskPriorityGet                  0

/* Co hieu luc ca Release; khong dung assert() cua libc/NDEBUG. */
#define configASSERT(condition) \
    do { \
        if (!(condition)) { \
            RTOS_AssertFailed(__FILE__, (uint32_t)__LINE__); \
        } \
    } while (0)

#endif
