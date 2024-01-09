#include <stdio.h>
#include <string.h>
#include <memory.h>
#include <time.h>
#include "esp_log.h"
#include "esp_sntp.h"


#include "log.h"
#include "comms.h"
#include "comms_events.h"
#include "wifi.h"

// Internal state for wifi and LTE
typedef struct
{
    bool wifi;
    bool lte;
    int connect_error_count; // Number of connect errors (2 = nothing available)
} _comms_status_t;
static _comms_status_t state;

#define LTE_TIMEOUT_MS 10000

#define MAX_BUFFER_LEN 512
#define MAX_RESPONSE_SIZE 512
typedef struct
{
    uint8_t data[MAX_RESPONSE_SIZE]; // Response data
    uint16_t length;                 // Response data length
} comms_event_arg_t;

// comms_event_arg_t lte_bridge_response;

static bool got_data = true;

bool comms_got_data()
{
    return got_data;
}

/**
 * Send and receive data. Max buffer size is MAX_BUFFER_LEN (512)
 */
int comms_send_receive(const uint8_t *data, const size_t len, uint8_t *response_buffer, uint16_t *response_length)
{
    int err;
    if (state.wifi)
    {
        ESP_LOGI(LOG_TAG_COMMS, "Sending message (%d bytes) via wifi", len);
        err = wifi_send_and_receive(data, len, response_buffer, response_length);
        if (err >= 0)
        {
            got_data = (*response_length > 0) ? true : false;
            return COMMS_OK;
        }
        if (!state.lte)
        {
            return err;
        }
    }

    // // Wi-Fi send failed. Try the LTE fallback
    // if (state.lte)
    // {
    //     ESP_LOGI(LOG_TAG_COMMS, "Sending mesage (%d bytes) via LTE", len);
    //     int ret = lte_send_and_receive(data, len, response_buffer, response_length);
    //     ESP_LOGI(LOG_TAG_COMMS, "Got %d bytes back!", (int)*response_length);
    //     got_data = (*response_length > 0) ? true : false;
    //     return ret;
    // }

    ESP_LOGE(LOG_TAG_COMMS, "WiFi and LTE are offline...");
    return COMMS_OFFLINE;
}

static void comms_event_handler(void *arg, esp_event_base_t event_base,
                                int32_t event_id, void *event_data)
{
    if (event_base != COMMS_EVENT)
    {
        return;
    }
    switch (event_id)
    {
    case COMMS_WIFI_ONLINE:
        state.wifi = true;
        break;
    case COMMS_WIFI_OFFLINE:
        state.wifi = false;
        break;
    case COMMS_WIFI_ERROR:
        state.wifi = false;
        state.connect_error_count++;
        if (state.connect_error_count == 2)
        {
            comms_event_post(COMMS_ERROR, NULL, 0);
        }
        break;
    // case COMMS_LTE_ONLINE:
    //     state.lte = true;
    //     break;
    // case COMMS_LTE_OFFLINE:
    //     state.lte = false;
    //     break;
    // case COMMS_LTE_ERROR:
    //     state.connect_error_count++;
    //     state.lte = false;
    //     if (state.connect_error_count == 2)
    //     {
    //         comms_event_post(COMMS_ERROR, NULL, 0);
    //     }
    //     break;
    default:
        // Ignore any other event
        break;
    }
}

void comms_log_init()
{
    esp_log_level_set(LOG_TAG_WIFI, CONFIG_COMMS_WIFI_LOG_LEVEL);
    esp_log_level_set(LOG_TAG_COMMS, CONFIG_COMMS_LOG_LEVEL);
    esp_log_level_set(LOG_COAP, CONFIG_COMMS_COAP_LOG_LEVEL);
}

esp_err_t comms_startup(comms_config_t *config)
{
    comms_log_init();

    esp_err_t res = comms_event_handler_init();
    if (res != ESP_OK)
    {
        return res;
    }
    state.wifi = false;
    state.lte = false;
    state.connect_error_count = 0;
    comms_register_event_handler(comms_event_handler, NULL);
    wifi_connect(config->ssid, config->password);
    // lte_connect();
    return ESP_OK;
}

static bool time_set = false;

void comms_set_time(const char *timestring)
{
    if (time_set)
    {
        return;
    }
    if (timestring == NULL)
    {
        // Set the time zone to CET (GMT-2 because reasons)
        // This should be CET but ESP-IDF doesn't know this
        setenv("TZ", "GMT-2", 1);
        tzset();
        ESP_LOGI(LOG_TAG_COMMS, "Using %s for NTP", NTP_HOST);
        esp_sntp_setoperatingmode(SNTP_OPMODE_POLL);
        esp_sntp_setservername(0, NTP_HOST);
        esp_sntp_init();

        time_t t = time(NULL);
        // The SNTP process might not finish right away. Wait until the time is set before returning.
        // (the time is slightly arbitrary, just something bigger than the system time that starts on 1)
        while (t < 330373220)
        {
            ESP_LOGW(LOG_TAG_COMMS, "Time is %ld, wait for SNTP to finish...", t);
            vTaskDelay(pdMS_TO_TICKS(1000));
            t = time(NULL);
        }
        time_set = true;
        comms_event_post(COMMS_CLOCK, NULL, 0);
        return;
    }

    //  Example: 22/09/19,12:30:46+08
    struct tm tm;
    if (NULL == strptime(timestring, "%y/%m/%d,%H:%M:%S", &tm))
    {
        ESP_LOGE(LOG_TAG_COMMS, "Failed parsing time string (%s)", timestring);
        return;
    }

    time_t t = mktime(&tm);

    struct timeval t_now;
    t_now.tv_sec = t;
    t_now.tv_usec = 0;

    if (0 != settimeofday(&t_now, NULL))
    {
        ESP_LOGE(LOG_TAG_COMMS, "Failed setting time from time string (%s)", timestring);
    }

    comms_event_post(COMMS_CLOCK, NULL, 0);
}