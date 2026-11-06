#include <stdint.h>

/* Startup calls this before .data/.bss initialization: do not use globals here.
 * Keep the reset clock configuration (HSI); build uses software floating point.
 */
void SystemInit(void)
{
    volatile uint32_t *vtor = (volatile uint32_t *)0xE000ED08U;
    *vtor = 0x08000000U;
    __asm volatile ("dsb" ::: "memory");
    __asm volatile ("isb" ::: "memory");
}

/* Minimal constructor initialization for a freestanding build without libc. */
typedef void (*InitFunction)(void);
extern InitFunction __preinit_array_start[];
extern InitFunction __preinit_array_end[];
extern InitFunction __init_array_start[];
extern InitFunction __init_array_end[];

void __libc_init_array(void)
{
    for (InitFunction *p = __preinit_array_start; p < __preinit_array_end; ++p)
        (*p)();
    for (InitFunction *p = __init_array_start; p < __init_array_end; ++p)
        (*p)();
}
