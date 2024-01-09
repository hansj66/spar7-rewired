#pragma once

#include "log.h"
#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include "esp_err.h"
#include "comms_error.h"
#include "comms_config.h"

/**
 * Communication interface for nRF91/WiFi comms
 */

// Too good to pass
#define NTP_HOST "ntp.justervesenet.no"

/**
 * True if last comms_send_receive returned new data
 */
bool comms_got_data();

/**
 * Send and receive data.
 */
int comms_send_receive(
    const uint8_t *data, const size_t len,
    uint8_t *resp, uint16_t *resp_len);

/**
 * Start the comms interfaces
 */
esp_err_t comms_startup(comms_config_t *config);

/**
 * Set clock. If the string is NULL (or empty) it will use SNTP. The clock will only be set once.
 */
void comms_set_time(const char *timestr);