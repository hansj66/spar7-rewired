#include "settings_spar7.h"
#include "nvs_flash.h"
#include "nvs.h"
#include "esp_log.h"

static const char* TAG = "settings";


#define DEFAULT_PAYOUT_DEFAULT_DELAY_MS     10
#define DEFAULT_COIN_EXIT_DEFAULT_DELAY_MS  10
#define DEFAULT_HOPPER_DEFAULT_DELAY_MS     300

int32_t PAYOUT_DELAY_MS    = DEFAULT_PAYOUT_DEFAULT_DELAY_MS;
int32_t COIN_EXIT_DELAY_MS = DEFAULT_COIN_EXIT_DEFAULT_DELAY_MS;
int32_t HOPPER_DELAY_MS    = DEFAULT_HOPPER_DEFAULT_DELAY_MS;

void set_debounce(setting_t s, int value)
{
    nvs_handle_t my_handle;
    esp_err_t err = nvs_open("nvs", NVS_READWRITE, &my_handle);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Error (%s) opening NVS handle!\n", esp_err_to_name(err));
        return;
    } 

    switch (s)
    {
        case PAYOUT_DEBOUNCE:       err = nvs_set_i32(my_handle, "payout_debounce", value);
                                    if (err != ESP_OK) 
                                    {
                                        ESP_LOGW(TAG, "Unable to store payout debounce value in NVS");
                                    }
                                    break;
        case COIN_EXIT_DEBOUNCE:    err = nvs_set_i32(my_handle, "exit_debounce", value);
                                    if (err != ESP_OK) 
                                    {
                                        ESP_LOGW(TAG, "Unable to store coin exit debounce value in NVS");
                                    }
                                    break;
        case HOPPER_DEBOUNCE:       err = nvs_set_i32(my_handle, "hopper_debounce", value);
                                    if (err != ESP_OK) 
                                    {
                                        ESP_LOGW(TAG, "Unable to store hopper debounce value in NVS");
                                    }
                                    break;
        default:                    ESP_LOGE(TAG, "Unknown setting: %d", (int)s);
    }
    err = nvs_commit(my_handle);
    if (err != ESP_OK) 
    {
        ESP_LOGE(TAG, "Unable to commit changes to NVS");
    }        
    

    nvs_close(my_handle);
 }

int get_debounce(setting_t s)
{
    nvs_handle_t my_handle;
    esp_err_t err = nvs_open("nvs", NVS_READWRITE, &my_handle);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Error (%s) opening NVS handle!\n", esp_err_to_name(err));
        return 0;
    } 

    switch (s)
    {
        case PAYOUT_DEBOUNCE:       err = nvs_get_i32(my_handle, "payout_debounce", &PAYOUT_DELAY_MS);
                                    nvs_close(my_handle);
                                    if (err != ESP_OK) 
                                    {
                                        ESP_LOGI(TAG, "Unable to fetch payout debounce value from NVS. Using default value of: %d", DEFAULT_PAYOUT_DELAY_MS);
                                        return DEFAULT_PAYOUT_DELAY_MS;
                                    }
                                    return PAYOUT_DELAY_MS;
                                    break;
        case COIN_EXIT_DEBOUNCE:    err = nvs_get_i32(my_handle, "exit_debounce", &COIN_EXIT_DELAY_MS);
                                    nvs_close(my_handle);
                                    if (err != ESP_OK) 
                                    {
                                        ESP_LOGI(TAG, "Unable to fetch coin exit debounce value from NVS. Using default value of: %d",DEFAULT_COIN_EXIT_DELAY_MS);
                                        return DEFAULT_COIN_EXIT_DELAY_MS;
                                    }
                                    return COIN_EXIT_DELAY_MS;
                                    break;
        case HOPPER_DEBOUNCE:       err = nvs_get_i32(my_handle, "hopper_debounce", &HOPPER_DELAY_MS);
                                    nvs_close(my_handle);
                                    if (err != ESP_OK) 
                                    {
                                        ESP_LOGI(TAG, "Unable to fetch hopper debounce value from NVS. Using default value of: %d", DEFAULT_HOPPER_DELAY_MS);
                                        return DEFAULT_HOPPER_DELAY_MS;
                                    }
                                    return HOPPER_DELAY_MS;
                                    break;                                    break;
        default:                    ESP_LOGE(TAG, "Unknown setting: %d", (int)s);
    }
    return 0;
}

void set_wifi(const char * ssid, const char * password)
{
    nvs_handle_t my_handle;
    esp_err_t err = nvs_open("nvs", NVS_READWRITE, &my_handle);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Error (%s) opening NVS handle!\n", esp_err_to_name(err));
        return;
    } 

    err = nvs_set_str(my_handle, "wifi_ssid", ssid);
    if (err != ESP_OK) 
    {
        ESP_LOGE(TAG, "Unable to store wifi ssid");
    }

    err = nvs_set_str(my_handle, "wifi_password", password);
    if (err != ESP_OK) 
    {
        ESP_LOGE(TAG, "Unable to store wifi password");
    }
    nvs_close(my_handle);
}

void get_string_value(char * key, char * value, size_t* length)
{
    nvs_handle_t my_handle;
    esp_err_t err = nvs_open("nvs", NVS_READWRITE, &my_handle);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Error (%s) opening NVS handle!\n", esp_err_to_name(err));
        *length = 0;
        return;
    } 

    err = nvs_get_str(my_handle, key, value, length);
    if (err != ESP_OK) 
    {
        ESP_LOGW(TAG, "No NVS entry for '%s'", key);
    }
    nvs_close(my_handle);
}


void get_wifi_ssid(char * ssid, size_t* length)
{
    get_string_value("wifi_ssid", ssid, length);
}

void get_wifi_password(char * password, size_t* length)
{
    get_string_value("wifi_password", password, length);
}


