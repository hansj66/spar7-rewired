#pragma once

#include <stdint.h>
#include <stdio.h>

#include "esp_err.h"

#define LAB5E_COAP_HOST "data.lab5e.com"
#define LAB5E_COAP_PORT "5684"

#define COAP_OK 0
#define COAP_OK_NO_CONTENT 1
#define COAP_CONNECT_ERROR -1
#define COAP_PACKET_INIT_ERROR -2
#define COAP_APPEND_PATH_ERROR -3
#define COAP_APPEND_CONTENT_FORMAT_ERROR -4
#define COAP_APPEND_PAYLOAD_MARKER_ERROR -5
#define COAP_APPEND_PAYLOAD_ERROR -6
#define COAP_SEND_ERROR -7
#define COAP_NO_RESPONSE_ERROR -8
#define COAP_PARSE_ERROR -9
#define COAP_RESPONSE_BUFFER_TOO_SMALL -10

// Coap send. If the buffer is 0 bytes long a GET will be sent to the
// server otherwise a POST is used.
int coap_client_send(const uint8_t *buf, const size_t len, uint8_t *response_buffer, uint16_t *response_length);