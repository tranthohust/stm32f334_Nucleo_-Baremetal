#ifndef IO_DEMO_H
#define IO_DEMO_H
#include <stdbool.h>
/* Cung main/Comm task voi communication demo; ISR chi bao su kien. */
void IO_Demo_Init(void);
void IO_Demo_Process(void);
bool IO_Demo_Command(const char *line);
void IO_Demo_GPIOIRQ(void);
#endif
