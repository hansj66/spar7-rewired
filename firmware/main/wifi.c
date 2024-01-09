#include "log.h"
#include "wifi.h"
#include "esp_log.h"

/* WiFi station Example

   This example code is in the Public Domain (or CC0 licensed, at your option.)

   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
*/
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "freertos/queue.h"
#include "esp_system.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "comms.h"
// #include "pill_nvs.h"

#include "lwip/err.h"
#include "lwip/sys.h"

#include "esp_wifi.h"
#include "comms_events.h"
#include "coap_client.h"

#if CONFIG_ESP_WIFI_AUTH_OPEN
#define ESP_WIFI_SCAN_AUTH_MODE_THRESHOLD WIFI_AUTH_OPEN
#elif CONFIG_ESP_WIFI_AUTH_WEP
#define ESP_WIFI_SCAN_AUTH_MODE_THRESHOLD WIFI_AUTH_WEP
#elif CONFIG_ESP_WIFI_AUTH_WPA_PSK
#define ESP_WIFI_SCAN_AUTH_MODE_THRESHOLD WIFI_AUTH_WPA_PSK
#elif CONFIG_ESP_WIFI_AUTH_WPA2_PSK
#define ESP_WIFI_SCAN_AUTH_MODE_THRESHOLD WIFI_AUTH_WPA2_PSK
#elif CONFIG_ESP_WIFI_AUTH_WPA_WPA2_PSK
#define ESP_WIFI_SCAN_AUTH_MODE_THRESHOLD WIFI_AUTH_WPA_WPA2_PSK
#elif CONFIG_ESP_WIFI_AUTH_WPA3_PSK
#define ESP_WIFI_SCAN_AUTH_MODE_THRESHOLD WIFI_AUTH_WPA3_PSK
#elif CONFIG_ESP_WIFI_AUTH_WPA2_WPA3_PSK
#define ESP_WIFI_SCAN_AUTH_MODE_THRESHOLD WIFI_AUTH_WPA2_WPA3_PSK
#elif CONFIG_ESP_WIFI_AUTH_WAPI_PSK
#define ESP_WIFI_SCAN_AUTH_MODE_THRESHOLD WIFI_AUTH_WAPI_PSK
#endif

static int s_retry_num = 0;
static void event_handler(void *arg, esp_event_base_t event_base,
                          int32_t event_id, void *event_data)
{
    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START)
    {
        esp_wifi_connect();
    }
    else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED)
    {
        if (s_retry_num < CONFIG_ESP_MAXIMUM_WIFI_RETRY)
        {
            esp_wifi_connect();
            s_retry_num++;
            ESP_LOGD(LOG_TAG_WIFI, "Retry wifi connection #%d", s_retry_num);
            comms_event_post(COMMS_WIFI_OFFLINE, NULL, 0);
        }
        else
        {
            ESP_LOGE(LOG_TAG_WIFI, "Error connecting WiFi");
            comms_event_post(COMMS_WIFI_ERROR, NULL, 0);
        }
    }
    else if (event_base == WIFI_EVENT && event_id == ESP_ERR_WIFI_TIMEOUT)
    {
        ESP_LOGE(LOG_TAG_WIFI, "WiFi connection Timeout");
        comms_event_post(COMMS_WIFI_ERROR, NULL, 0);
    }
    else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP)
    {
        ip_event_got_ip_t *event = (ip_event_got_ip_t *)event_data;
        ESP_LOGI(LOG_TAG_WIFI, "Got ip:" IPSTR, IP2STR(&event->ip_info.ip));

        wifi_info_t args;
        sprintf(args.wifi_ip, IPSTR, IP2STR(&event->ip_info.ip));
        comms_event_post(COMMS_WIFI_ONLINE, &args, sizeof(wifi_info_t));
        comms_set_time(NULL);
        s_retry_num = 0;
    }
}

void wifi_init_sta(const char *ssid, const char *password)
{
    ESP_ERROR_CHECK(esp_netif_init());

    ESP_ERROR_CHECK(esp_event_loop_create_default());
    esp_netif_create_default_wifi_sta();

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    esp_event_handler_instance_t instance_any_id;
    esp_event_handler_instance_t instance_got_ip;
    ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT,
                                                        ESP_EVENT_ANY_ID,
                                                        &event_handler,
                                                        NULL,
                                                        &instance_any_id));
    ESP_ERROR_CHECK(esp_event_handler_instance_register(IP_EVENT,
                                                        IP_EVENT_STA_GOT_IP,
                                                        &event_handler,
                                                        NULL,
                                                        &instance_got_ip));

    wifi_config_t wifi_config =
        {
            .sta =
                {
                    /* Setting a password implies station will connect to all security modes including WEP/WPA.
                     * However these modes are deprecated and not advisable to be used. Incase your Access point
                     * doesn't support WPA2, these mode can be enabled by commenting below line */
                    .threshold.authmode = ESP_WIFI_SCAN_AUTH_MODE_THRESHOLD,
                },
        };
    strcpy((char *)&(wifi_config.sta.ssid[0]), ssid);
    strcpy((char *)&(wifi_config.sta.password[0]), password);

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config));
    ESP_ERROR_CHECK(esp_wifi_start());

    ESP_LOGD(LOG_TAG_WIFI, "wifi_init_sta finished.");
}

#define MAX_WIFI_BUF_SIZE 512

typedef struct
{
    const uint8_t *txbuf;
    uint8_t *rxbuf;
    uint16_t txsize;
    uint16_t *rxsize;
    int result;
} rxtxBuffer_t;

static QueueHandle_t send_queue;
static QueueHandle_t receive_queue;
static SemaphoreHandle_t wifi_sem;

static void send_receive_task();

void wifi_connect(const char *ssid, const char *password)
{
    if ((NULL == ssid) || (0 == strlen(ssid)))
    {
        ESP_LOGE(LOG_TAG_WIFI, "Unable to connect to WiFi - no SSID");
        comms_event_post(COMMS_WIFI_ERROR, NULL, 0);
        return;
    }
    ESP_LOGI(LOG_TAG_WIFI, "Start WiFi connection");
    wifi_init_sta(ssid, password);
    send_queue = xQueueCreate(1, sizeof(rxtxBuffer_t));
    receive_queue = xQueueCreate(1, sizeof(int));
    wifi_sem = xSemaphoreCreateBinary();
    xSemaphoreGive(wifi_sem);
    TaskHandle_t rxtxTaskHandle = NULL;
    // Pin the task to the core not running the LVGL thread. The DTLS code is quite resource intensive and
    // makes the UI stutter when it connects and disconnects.
    xTaskCreatePinnedToCore(send_receive_task, "wifi_rxtx", 12 * 1024, NULL, 6, &rxtxTaskHandle, 1);
    configASSERT(rxtxTaskHandle);
}

/*
 * The send and receive function uses two queues to pass the request and response buffers to a
 * separate CoAP/DTLS thread since DTLS requires a fair bit of stack space to run. Rather than setting
 * aside stack space in *all* threads we just use one thread with a (relatively) large stack- This will
 * also make it easier to pin the thread to a different core from the UI thread.
 */

int wifi_send_and_receive(const uint8_t *txbuf, const size_t txlen, uint8_t *rxbuf, uint16_t *rxlen)
{
    xSemaphoreTake(wifi_sem, portMAX_DELAY);
    rxtxBuffer_t tx_q_buf = {.txbuf = txbuf, .txsize = txlen, .rxbuf = rxbuf, .rxsize = rxlen};
    if (xQueueSend(send_queue, &tx_q_buf, pdMS_TO_TICKS(10000)) != pdTRUE)
    {
        ESP_LOGE(LOG_TAG_COMMS, "Error posting to CoAP tx queue");
        *rxlen = 0;
        xSemaphoreGive(wifi_sem);
        return COAP_SEND_ERROR;
    }

    int result = 0;
    if (xQueueReceive(receive_queue, &result, pdMS_TO_TICKS(60000)) != pdTRUE)
    {
        ESP_LOGE(LOG_TAG_COMMS, "Error receiving from CoAP rx queue");
        *rxlen = 0;
        xSemaphoreGive(wifi_sem);
        return COAP_SEND_ERROR;
    }

    xSemaphoreGive(wifi_sem);
    return result;
}

static void send_receive_task()
{
    while (1)
    {
        rxtxBuffer_t txbuffer;
        if (xQueueReceive(send_queue, &txbuffer, pdMS_TO_TICKS(10000)) != pdTRUE)
        {
            continue;
        }
        int result = coap_client_send(txbuffer.txbuf, txbuffer.txsize, txbuffer.rxbuf, txbuffer.rxsize);
        xQueueSend(receive_queue, &result, pdMS_TO_TICKS(10000));
    }
}