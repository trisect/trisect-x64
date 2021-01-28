#ifndef STDINT_H
#define STDINT_H
#include <stdint.h>
#endif

#ifndef EFIMEMORY_H
#define EFIMEMORY_H
#include "efimemory.h"
#endif

#ifndef TRYTE_H
#define TRYTE_H
#include "tryte.h"
#endif

#ifndef BOOT_H
#define BOOT_H
#include "boot.h"
#endif

uint64_t get_memory_size(EFI_MEMORY_DESCRIPTOR *map, uint64_t mapEntries, uint64_t mapDescriptorSize) {
    static uint64_t memorySize = 0;
    if(memorySize) return memorySize;
    
    for(uint64_t i = 0; i < mapEntries; i++) {
        EFI_MEMORY_DESCRIPTOR *descriptor = (EFI_MEMORY_DESCRIPTOR*)((uint64_t)map + (i * mapDescriptorSize));
        memorySize += descriptor->pageCount * PAGE_TRYTE;
    }
    return memorySize;
}

__tryte_buffer_ret tryte_at(__tryte_buffer_ptr(t)) {
    return (__tryte_buffer_ret)((uint64_t)t * BYTE_TRIT / TRYTE_TRIT * TRYTE_TRIT / BYTE_TRIT);
}

__tryte_buffer_ret next_tryte_at(__tryte_buffer_ptr(t)) {
    return (__tryte_buffer_ret)(t + (TRYTE_TRIT - (uint64_t)t * BYTE_TRIT % TRYTE_TRIT) * TRIT_BIT / CHAR_BIT);
}

void set_tryte(void *address, __tryte(t)) {
    uint8_t offset = 0xff >> ((uint64_t)address * BYTE_TRIT / TRYTE_TRIT % BYTE_TRIT * TRIT_BIT);
    ((__tryte_buffer_ret)address)[0] &= ~offset;
    ((__tryte_buffer_ret)address)[0] |= t[0] & offset;
    ((__tryte_buffer_ret)address)[1] = t[1];
    offset >>= 1;
    ((__tryte_buffer_ret)address)[2] &= ~offset;
    ((__tryte_buffer_ret)address)[2] |= t[2] & offset;
}

__tryte_buffer_ret get_tryte(void *address) {
    static __tryte(t);
    set_tryte(&t, tryte_at(address));
    return t;
}

void memset(void *start, __tryte(value), uint64_t num) {
    start = tryte_at(start);
    for(uint64_t i = 0; i < num; i++)
        set_tryte(start + i * TRYTE_BYTE, value);
}