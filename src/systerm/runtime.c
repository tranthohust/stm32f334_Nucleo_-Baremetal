#include <stddef.h>

/* Freestanding (-nostdlib) van cho phep compiler phat sinh memcpy/memset
 * khi copy/clear struct. Day chi la memory helper, khong keo libc/heap vao.
 * CMake tat loop-distribute-patterns cho file nay de tranh GCC bien vong
 * lap trong memset thanh mot loi goi memset de quy. */
void *memset(void *destination, int value, size_t length)
{
    unsigned char *out = destination;
    for (size_t i = 0U; i < length; ++i)
        out[i] = (unsigned char)value;
    return destination;
}

void *memcpy(void *destination, const void *source, size_t length)
{
    unsigned char *out = destination;
    const unsigned char *in = source;
    for (size_t i = 0U; i < length; ++i)
        out[i] = in[i];
    return destination;
}
