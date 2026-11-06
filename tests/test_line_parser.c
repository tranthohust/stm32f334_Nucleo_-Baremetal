#include "line_parser.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

typedef struct {
    unsigned count;
    char lines[8][LINE_PARSER_CAPACITY];
} Capture;

static void capture(const char *line, size_t length, void *context)
{
    Capture *output = context;
    assert(output->count < 8U);
    assert(length < LINE_PARSER_CAPACITY);
    memcpy(output->lines[output->count], line, length + 1U);
    ++output->count;
}

static void feed(LineParser *parser, const char *text)
{
    LineParser_Feed(parser, (const uint8_t *)text, strlen(text));
}

int main(void)
{
    Capture output = {0};
    LineParser parser;
    LineParser_Init(&parser, capture, &output);

    /* Mot command bi tach qua nhieu DMA/IRQ batch, CRLF tach rieng. */
    feed(&parser, "LED ");
    feed(&parser, "ON\r");
    assert(output.count == 0U);
    feed(&parser, "\nPING\n\n");
    assert(output.count == 2U);
    assert(strcmp(output.lines[0], "LED ON") == 0);
    assert(strcmp(output.lines[1], "PING") == 0);

    /* Gioi han dung: 63 ky tu hop le, 64 ky tu thi bo ca dong. */
    char boundary[LINE_PARSER_CAPACITY + 1U];
    memset(boundary, 'A', sizeof boundary);
    boundary[LINE_PARSER_CAPACITY - 1U] = '\n';
    LineParser_Feed(&parser, (const uint8_t *)boundary, LINE_PARSER_CAPACITY);
    assert(output.count == 3U);
    assert(strlen(output.lines[2]) == LINE_PARSER_CAPACITY - 1U);
    memset(boundary, 'B', sizeof boundary);
    boundary[LINE_PARSER_CAPACITY] = '\n';
    LineParser_Feed(&parser, (const uint8_t *)boundary, sizeof boundary);
    assert(output.count == 3U && parser.rejected_lines == 1U);
    feed(&parser, "PING\n");
    assert(output.count == 4U);

    /* Mat byte khong duoc bien 'LED O' + 'N' thanh lenh LED ON. */
    feed(&parser, "LED O");
    LineParser_MarkDiscontinuity(&parser);
    feed(&parser, "N\nPING\n");
    assert(output.count == 5U && strcmp(output.lines[4], "PING") == 0);

    /* NUL trong dong khong duoc cat chuoi de thuc thi prefix. */
    static const uint8_t malformed[] = {'P', 'I', 'N', 'G', 0U, 'X', '\n'};
    LineParser_Feed(&parser, malformed, sizeof malformed);
    assert(output.count == 5U);
    feed(&parser, "PING\n");
    assert(output.count == 6U && parser.rejected_lines == 3U);

    /* CR giua dong khong duoc tu dong bien mat: khong noi hai tu thanh lenh. */
    feed(&parser, "LED\rON\n");
    assert(output.count == 7U && strcmp(output.lines[6], "LED\rON") == 0);
    puts("line_parser: PASS (fragmentation, CRLF, limits, loss, malformed input)");
    return 0;
}
