#include <stdint.h>

uint64_t lowc_foreign_add(uint64_t left, uint64_t right)
{
    return left + right;
}

uint64_t lowc_foreign_length(const unsigned char *text)
{
    uint64_t length = 0;

    while (text[length] != 0)
        length++;

    return length;
}

const unsigned char *lowc_foreign_identity(const unsigned char *text)
{
    return text;
}

uint8_t lowc_foreign_unsigned_byte(void)
{
    return 200;
}

int8_t lowc_foreign_signed_byte(void)
{
    return -7;
}
