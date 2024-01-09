/* 
    Wrappers for Zephyr functions that lives elsewhere in ESP-IDF/FreeRTOS.
*/
#include "z_funcs.h"
#include "esp_random.h"

uint32_t sys_rand32_get() {
    return esp_random();
}

void sys_rand_get(void *dst, size_t len) {
    esp_fill_random(dst, len);
}

uint32_t k_uptime_get_32() {
    // will use task tick count - this is roughly equivalent to the uptime (FSVO "uptime")
    return (uint32_t) xTaskGetTickCount();
}
