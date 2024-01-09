/**
 * CoAP client code. This generates a new task and queue that will handle
 * requests. Requests are pulled from a queue and the responses are returned via
 * callbacks
 */

#include "coap_client.h"

#include <string.h>

#include "log.h"
#include "bootloader_random.h"
#include "dtls.h"
#include "esp_random.h"
#include "z_coap.h"
#include "esp_log.h"

#define TOKEN_SIZE 4
#define MAX_BUFFER_LEN 512

static uint8_t work_buffer[MAX_BUFFER_LEN];

static dtls_state_t dtls;
static bool connected = false;

void coap_client_init()
{
    // Enable the RNG
    bootloader_random_enable();
    if (!dtls_setup(&dtls))
    {
        ESP_LOGE(LOG_COAP, "Error setting up DTLS");
    }
}

int handle_coap_error(int error)
{
    dtls_close(&dtls);
    connected = false;
    return error;
}

static bool init = false;

int coap_client_send(const uint8_t *buf, const size_t len, uint8_t *response_buffer, uint16_t *response_length)
{
    if (!init)
    {
        coap_client_init();
        init = true;
    }
    if (connected)
    {
        dtls_close(&dtls);
        connected = false;
    }
    if (!connected)
    {
        ESP_LOGD(LOG_COAP, "Setting up DTLS connection...");
        if (!dtls_connect(&dtls, LAB5E_COAP_HOST, LAB5E_COAP_PORT))
        {
            ESP_LOGE(LOG_COAP, "Error connecting to CoAP server");
            return COAP_CONNECT_ERROR;
        }
        connected = true;
    }

    struct coap_packet pdu;

    int message_id = coap_next_id();
    uint8_t token[TOKEN_SIZE];
    esp_fill_random(token, TOKEN_SIZE);
    uint8_t method = COAP_METHOD_GET;
    if (len > 0 && buf != NULL)
    {
        method = COAP_METHOD_POST;
    }
    if (coap_packet_init(&pdu, work_buffer, sizeof(work_buffer),
                         COAP_VERSION_1, COAP_TYPE_NON_CON, TOKEN_SIZE, token,
                         method, message_id) < 0)
    {
        ESP_LOGE(LOG_COAP, "Error initializing CoAP packet");
        return handle_coap_error(COAP_PACKET_INIT_ERROR);
    }

    if (coap_packet_append_option(&pdu, COAP_OPTION_URI_PATH,
                                  (const uint8_t *)"pill", strlen("pill")) < 0)
    {
        ESP_LOGE(LOG_COAP, "Error appending path option to request");
        return handle_coap_error(COAP_APPEND_PATH_ERROR);
    }

    if (method == COAP_METHOD_POST)
    {
        uint8_t content_format = COAP_CONTENT_FORMAT_APP_OCTET_STREAM;
        if (coap_packet_append_option(&pdu, COAP_OPTION_CONTENT_FORMAT,
                                      &content_format,
                                      sizeof(content_format)) < 0)
        {
            ESP_LOGE(LOG_COAP, "Error appending content format option");
            return handle_coap_error(COAP_APPEND_CONTENT_FORMAT_ERROR);
        }
        if (coap_packet_append_payload_marker(&pdu) < 0)
        {
            ESP_LOGE(LOG_COAP, "Error appending payload marker");
            return handle_coap_error(COAP_APPEND_PAYLOAD_MARKER_ERROR);
        }
        if (coap_packet_append_payload(&pdu, buf, len) < 0)
        {
            ESP_LOGE(LOG_COAP, "Error appending payload");
            return handle_coap_error(COAP_APPEND_PAYLOAD_ERROR);
        }
    }

    ESP_LOGD(LOG_COAP, "Sending message... (%d bytes)", pdu.offset);
    if (!dtls_send(&dtls, work_buffer, pdu.offset))
    {
        ESP_LOGE(LOG_COAP, "Error sending CoAP message");
        return handle_coap_error(COAP_SEND_ERROR);
    }

    ESP_LOGD(LOG_COAP, "Receiving response...");

    size_t received = dtls_receive(&dtls, work_buffer, sizeof(work_buffer));

    if (received == 0)
    {
        ESP_LOGE(LOG_COAP, "No response from server");
        return handle_coap_error(COAP_NO_RESPONSE_ERROR);
    }
    ESP_LOGD(LOG_COAP, "Parsing response (%d bytes)...", received);

    struct coap_packet reply;
    if (coap_packet_parse(&reply, work_buffer, received, NULL, 0) < 0)
    {
        ESP_LOGE(LOG_COAP, "Error parsing response");
        return handle_coap_error(COAP_PARSE_ERROR);
    }

    // Server responds with COAP_RESPONSE_CODE_CONTENT if there's a response
    // If there is no content we'll get a COAP_RESPONSE_CODE_VALID (or OK)
    if (coap_header_get_code(&reply) == COAP_RESPONSE_CODE_CONTENT)
    {
        // Content to process
        memset(response_buffer, 0, *response_length);

        uint16_t payload_length = 0;
        const uint8_t *buf = coap_packet_get_payload(&reply, &payload_length);
        if (payload_length > *response_length)
        {
            ESP_LOGE(LOG_COAP, "Woops, a slight case of possible buffer overrun");
            *response_length = 0;
            return handle_coap_error(COAP_RESPONSE_BUFFER_TOO_SMALL);
        }
        memcpy((void *)response_buffer, (void *)buf, payload_length);
        *response_length = (uint16_t)payload_length;
        return COAP_OK;

        // Ack response if required
        if (coap_header_get_type(&reply) == COAP_TYPE_CON)
        {
            ESP_LOGW(LOG_COAP,
                     "Got a confirmable message response; won't confirm it");
            // TODO: Ack the response in the next request
        }
    }
    *response_length = 0;
    return COAP_OK_NO_CONTENT;
}