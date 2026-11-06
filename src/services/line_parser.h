#ifndef LINE_PARSER_H
#define LINE_PARSER_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define LINE_PARSER_CAPACITY 64U
typedef void (*LineParser_Callback)(const char *line, size_t length, void *context);

typedef struct {
    char line[LINE_PARSER_CAPACITY];
    size_t length;
    bool discarding;
    uint32_t rejected_lines;
    LineParser_Callback callback;
    void *context;
} LineParser;

/* Parser khong biet USART/DMA: co the test ngay tren PC.
 * Callback chay dong bo trong Feed, vi vay Feed chi duoc goi tu main.
 * Con tro line chi hop le trong thoi gian callback. */
void LineParser_Init(LineParser *parser, LineParser_Callback callback, void *context);
void LineParser_Feed(LineParser *parser, const uint8_t *data, size_t length);
/* Mat byte => bo phan con lai cua dong, cho '\n' de dong bo lai. */
void LineParser_MarkDiscontinuity(LineParser *parser);

#endif
