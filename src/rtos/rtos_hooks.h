#ifndef RTOS_HOOKS_H
#define RTOS_HOOKS_H

#include <stdint.h>

enum {
    RTOS_FAULT_NONE = 0U,
    RTOS_FAULT_ASSERT = 1U,
    RTOS_FAULT_STACK_OVERFLOW = 2U,
    RTOS_FAULT_STARTUP = 3U
};

/* Xem bang debugger khi PA5 sang lien tuc. Giu nguyen loi DAU TIEN.
 * Cac pointer chi duoc luu de chan doan; fault handler khong dereference.
 */
extern volatile uint32_t g_rtos_fault_code;
extern volatile uint32_t g_rtos_fault_line;
extern const char *volatile g_rtos_fault_file;
extern volatile uintptr_t g_rtos_fault_task;
extern const char *volatile g_rtos_fault_task_name;

void RTOS_Fatal(uint32_t code) __attribute__((noreturn));
void RTOS_AssertFailed(const char *file, uint32_t line) __attribute__((noreturn));

#endif
