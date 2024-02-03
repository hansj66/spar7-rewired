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

#include "lwip/err.h"
#include "lwip/sys.h"

#include "esp_wifi.h"
#include "settings_spar7.h"
#include "nvs_flash.h"
#include "nvs.h"

static const char *TAG = "wifi";

char ip_address[16] = {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0};
bool is_online = false;

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


bool is_wifi_online()
{
    return is_online;
}


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
            ESP_LOGD(TAG, "Retry wifi connection #%d", s_retry_num);
        }
        else
        {
            ESP_LOGE(TAG, "Error connecting WiFi");
        }
    }
    else if (event_base == WIFI_EVENT && event_id == ESP_ERR_WIFI_TIMEOUT)
    {
        ESP_LOGE(TAG, "WiFi connection Timeout");
    }
    else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP)
    {
        ip_event_got_ip_t *event = (ip_event_got_ip_t *)event_data;
        ESP_LOGI(TAG, "Got ip:" IPSTR, IP2STR(&event->ip_info.ip));
        sprintf(ip_address, IPSTR, IP2STR(&event->ip_info.ip));
        is_online = true;
        s_retry_num = 0;
    }
}

char wifi_ssid[SSID_MAX_LENGTH];
char wifi_password[PASSWORD_MAX_LENGTH];

void wifi_init_sta()
{
    nvs_handle_t my_handle;
    esp_err_t err = nvs_open("nvs", NVS_READWRITE, &my_handle);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Error (%s) opening NVS handle!\n", esp_err_to_name(err));
        return;
    } 

    size_t length;
    length = SSID_MAX_LENGTH;
    err = nvs_get_str(my_handle, "wifi_ssid", wifi_ssid, &length);
    if (err != ESP_OK) 
    {
        ESP_LOGW(TAG, "No NVS entry for 'wifi_ssid'. Unable to connect. Set SSID suing 'set_wifi <ssid> <password> shell command.");
        nvs_close(my_handle);
        return;
    }

    length = PASSWORD_MAX_LENGTH;
    err = nvs_get_str(my_handle, "wifi_password", wifi_password, &length);
    if (err != ESP_OK) 
    {
        ESP_LOGW(TAG, "No NVS entry for 'wifi_password'. Unable to connect. Set SSID suing 'set_wifi <ssid> <password> shell command.");
        nvs_close(my_handle);
        return;
    }

    nvs_close(my_handle);



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
    strcpy((char *)&(wifi_config.sta.ssid[0]), wifi_ssid);
    strcpy((char *)&(wifi_config.sta.password[0]), wifi_password);

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config));
    ESP_ERROR_CHECK(esp_wifi_start());

    ESP_LOGD(TAG, "wifi_init_sta finished.");
}

void wifi_connect()
{
    ESP_LOGI(TAG, "Start WiFi connection");
    wifi_init_sta();
}

