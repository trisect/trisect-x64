#ifndef STDINT_H
#define STDINT_H
#include <stdint.h>
#endif

#ifndef STDBOOL_H
#define STDBOOL_H
#include <stdbool.h>
#endif

#ifndef LIMITS_H
#define LIMITS_H
#include <limits.h>
#endif

// Bitmap struct
typedef struct {
    size_t size;
    uint8_t *buffer;
} BITMAP;

// Read bit from bitmap
bool read_bit(uint8_t *buffer, uint64_t index) {
    return (buffer[index / CHAR_BIT] & 1 << (CHAR_BIT - index) % CHAR_BIT) ? true : false;
}

// Write bit to bitmap at index
void write_bit(uint8_t *buffer, uint64_t index, bool bit) {
    buffer[index / CHAR_BIT] &= ~(1 << (CHAR_BIT - index) % CHAR_BIT);
    buffer[index / CHAR_BIT] |= bit << (CHAR_BIT - index) % CHAR_BIT;
}