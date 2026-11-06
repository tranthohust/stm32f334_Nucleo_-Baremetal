#include "line_parser.h"

void LineParser_Init(LineParser *parser, LineParser_Callback callback, void *context)
{
    parser->length = 0U;
    parser->discarding = false;
    parser->rejected_lines = 0U;
    parser->callback = callback;
    parser->context = context;
    parser->line[0] = '\0';
}

void LineParser_MarkDiscontinuity(LineParser *parser)
{
    parser->length = 0U;
    if (!parser->discarding) ++parser->rejected_lines;
    parser->discarding = true;
}

void LineParser_Feed(LineParser *parser, const uint8_t *data, size_t length)
{
    for (size_t i = 0U; i < length; ++i) {
        uint8_t byte = data[i];
        if (byte == '\n') {
            if (!parser->discarding) {
                /* Ho tro terminal gui CRLF; chi bo CR o CUOI dong. */
                if (parser->length != 0U && parser->line[parser->length - 1U] == '\r')
                    --parser->length;
                parser->line[parser->length] = '\0';
                if (parser->length != 0U && parser->callback != NULL)
                    parser->callback(parser->line, parser->length, parser->context);
            }
            parser->length = 0U;
            parser->discarding = false;
        } else if (!parser->discarding) {
            /* Khong cho NUL/control byte bien lenh hu thanh mot lenh hop le.
             * Dong dai: bo TOAN BO dong, khong thuc thi phan prefix. */
            if ((byte < 32U && byte != '\r') || byte > 126U ||
                parser->length == LINE_PARSER_CAPACITY - 1U)
                LineParser_MarkDiscontinuity(parser);
            else
                parser->line[parser->length++] = (char)byte;
        }
    }
}
