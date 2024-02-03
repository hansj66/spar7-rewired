// Copyright 2024 Hans Jørgen Grimstad

// Licensed under the Apache License, Version 2.0 (the "License"); you may not use 
// this file except in compliance with the License. You may obtain a copy of the 
// License at http://www.apache.org/licenses/LICENSE-2.0 Unless required by applicable 
// law or agreed to in writing, software distributed under the License is distributed 
// on an "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied. 
// See the License for the specific language governing permissions and limitations under the License.

#include <stdio.h>
#include <inttypes.h>
#include "sdkconfig.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/task.h"
#include "esp_chip_info.h"
#include "esp_flash.h"
#include "esp_system.h"
#include "sys/time.h"
#include "filesystem.h"
#include "esp_log.h"
#include "soc/soc.h"
#include "soc/gpio_reg.h"
#include "console.h"
#include "nvs.h"
#include "nvs_flash.h"
#include "settings_spar7.h"
#include "hal_spar7.h"
#include "wifi.h"
#include "dtls.h"

const uint8_t FIRMWARE_VERSION_MAJOR = 1;
const uint8_t FIRMWARE_VERSION_MINOR = 0;

#define HOST "data.lab5e.com"
#define PORT "1234"

static const char* TAG = "SPAR7";

extern int PAYOUT_DELAY_MS;
extern int COIN_EXIT_DELAY_MS;
extern int HOPPER_DELAY_MS;

// Task priorities (higher numbers == higher priorities)
//
#define CONSOLE_TASK_PRIORITY 10
#define GAME_TASK_PRIORITY 10
#define SEND_TASK_PRIORITY 20
// Max queue sizes
#define MAX_COIN_QUEUE_SIZE 20


void send_bookkeeping_data(int8_t data);

// Bookkeeping
//
int total_coins_in = 0;
int total_coins_out = 0;

// Queues
//
static QueueHandle_t coin_evt_queue = NULL;

void read_debounce_settings_from_nvs()
{
    get_debounce(PAYOUT_DEBOUNCE);
    get_debounce(COIN_EXIT_DEBOUNCE);
    get_debounce(HOPPER_DEBOUNCE);
}

static void console_task(void * arg)
{
    run_console();
}

static void game_task(void * arg)
{
    // Initialize display
    display_digit(0);

    uint32_t regs = REG_READ(GPIO_IN_REG);
    while  ((1ULL << GPIO_HOPPER) & ~regs)
    {
        regs = REG_READ(GPIO_IN_REG);
        start_motor();
    }
    stop_motor();

    
    while (true)
    {
        uint32_t regs = REG_READ(GPIO_IN_REG);

        int8_t payout = 0;

        if  ((1ULL << GPIO_PAY2) & ~regs)
            payout = 2;
        if  ((1ULL << GPIO_PAY3) & ~regs)
            payout = 3;
        if  ((1ULL << GPIO_PAY4) & ~regs)
            payout = 4;
        if  ((1ULL << GPIO_PAY7) & ~regs)
            payout = 7;
        if  ((1ULL << GPIO_COIN_EXIT) & ~regs)
        {
            // TODO: send message: coins in + 1 
        }

        if  (payout != 0)
        {
            printf("Payout: %d\n", payout);
            int countdown = payout;
            display_digit(countdown);

            send_bookkeeping_data(-payout);
            
            // Wait for coin exit
            for (;;)
            {
                regs = REG_READ(GPIO_IN_REG);
                if  ((1ULL << GPIO_COIN_EXIT) & ~regs)
                {
                    send_bookkeeping_data((int8_t)1);
                    printf("Coin exit\n");
                    break;
                }
                vTaskDelay(COIN_EXIT_DELAY_MS / portTICK_PERIOD_MS);
            }
      
            // Do payout
            start_motor();
            for (int i=0; i<payout; i++)
            {
                for (;;) 
                {
                    // wait for hopper
                    regs = REG_READ(GPIO_IN_REG);
                    if  ((1ULL << GPIO_HOPPER) & ~regs)
                    {
                        // Increment earnings by 1
                        printf("Hopper\n");

                        // Only debounce first n-1 coins
                        if (countdown >= 1)
                        {
                            vTaskDelay(HOPPER_DELAY_MS / portTICK_PERIOD_MS);
                        }
                       
                        countdown--;
                        display_digit(countdown);

                        if (countdown == 0)
                        {
                            break;
                        }
                    }
                    if (countdown == 0)
                    {
                        break;
                    }
                    vTaskDelay(1 / portTICK_PERIOD_MS);
                }

            }

            // Run motor until switch opens again
            regs = REG_READ(GPIO_IN_REG);
            while  ((1ULL << GPIO_HOPPER) & ~regs)
            {
                regs = REG_READ(GPIO_IN_REG);
            }
            vTaskDelay(10 / portTICK_PERIOD_MS);
            stop_motor();            
        }

        vTaskDelay(PAYOUT_DELAY_MS / portTICK_PERIOD_MS);
    }
}

void send_bookkeeping_data(int8_t data)
{
    if (is_wifi_online())
    {
        ESP_LOGI(TAG, "Sending bookkeeping data.");
        xQueueSend(coin_evt_queue, &data,  pdMS_TO_TICKS(1000));
    }
    else
    {
        ESP_LOGI(TAG, "Unable to send bookkeeping data (offline)");
    }
}


static void send_task(void * arg)
{
    // Block until we're online and ready to sedn bookkeeping data
    while (!is_wifi_online())
    {
        vTaskDelay(10 / portTICK_PERIOD_MS);
    }

     dtls_state_t dtls;

    if (!dtls_connect(&dtls, HOST, PORT)) 
    {
        dtls_close(&dtls);
    }
  
    printf("Connected to %s:%s\n", HOST, PORT);

   while (true)
    {
        int8_t data = 0;
        if (pdTRUE == xQueueReceive(coin_evt_queue, &data, portMAX_DELAY))
        {
            if (!dtls_send(&dtls, &data, 1)) 
            {
                ESP_LOGE(TAG, "Failed to send bookkeeping data.");
            }
            ESP_LOGI(TAG, "Sent message %02X", data);
        }
    }

    dtls_close(&dtls);  // Probably not happening anytome soon...
    printf("Closed connection\n");
}


static void initialize_nvs(void)
{
    esp_err_t err = nvs_flash_init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK( nvs_flash_erase() );
        err = nvs_flash_init();
    }
    ESP_ERROR_CHECK(err);
}


void app_main(void)
{
    // Initialize NVS
    initialize_nvs();
    // Get delay settings
    read_debounce_settings_from_nvs();
    // Connect to wifi (if we have the credentials stored in NVS)
    wifi_connect();
    // Ini tialize GPIO
    init_gpio();
    // Initialize file system
    initialize_spiffs();

    // Create a queue to handle coin events
    coin_evt_queue =  xQueueCreate(MAX_COIN_QUEUE_SIZE, sizeof(int8_t));

    // start console task
    xTaskCreate(console_task, "Console Task", 8192, NULL, CONSOLE_TASK_PRIORITY, NULL);
    // start game task
    xTaskCreate(game_task, "Game Task", 4096, NULL, GAME_TASK_PRIORITY, NULL);
    // start send task
    xTaskCreate(send_task, "Send Task", 8192, NULL, SEND_TASK_PRIORITY, NULL);

    while (true)
    {
        vTaskDelay(10 / portTICK_PERIOD_MS);
    }
}

