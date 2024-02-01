#pragma once

#include <stdio.h>
#include <stdbool.h>
#include <stdint.h>

/**
 * Connect the WiFi adapter. When the wifi adapter is connected and receives an IP the COMMS_WIFI_ONLINE
 * event is raised.
 */
void wifi_connect();

/**
 * Send and recevie WiFi data. The txbuf and txlen parameters holds the output data, 0 if there is none.
 * rxbuf holds the returned bytes from the Span service and rxlen indicates then length of the rx buffer.
 *
 * On success the rxlen parameter is updated with the number of bytes read.
 *
 * This returns a zero or positive value on success and a negative value on error.
 */
int wifi_send_and_receive(const uint8_t *txbuf, const size_t txlen, uint8_t *rxbuf, uint16_t *rxlen);