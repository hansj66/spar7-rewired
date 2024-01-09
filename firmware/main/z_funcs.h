#pragma once

#include <stdint.h>
#include <sys/socket.h>

uint32_t sys_rand32_get();
void sys_rand_get(void *dst, size_t len);

#define MIN(x, y) ((x) < (y) ? (x) : (y))
#define MAX(x, y) ((x) > (y) ? (x) : (y))

// byteorder.h
static inline void sys_put_be16(uint16_t val, uint8_t dst[2])
{
    dst[0] = val >> 8;
    dst[1] = val;
}

static inline void sys_put_be32(uint32_t val, uint8_t dst[4])
{
    sys_put_be16(val >> 16, dst);
    sys_put_be16(val, &dst[2]);
}

// system uptime
uint32_t k_uptime_get_32();

// math_extras_impl.h
static inline bool u16_add_overflow(uint16_t a, uint16_t b, uint16_t *result)
{
    uint16_t c = a + b;
    *result = c;
    return c < a;
}