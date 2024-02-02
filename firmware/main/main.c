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

const uint8_t FIRMWARE_VERSION_MAJOR = 1;
const uint8_t FIRMWARE_VERSION_MINOR = 0;


static const char* LOGTAG = "SPAR7";

extern int PAYOUT_DELAY_MS;
extern int COIN_EXIT_DELAY_MS;
extern int HOPPER_DELAY_MS;

// Task priorities (higher numbers == higher priorities)
//
#define CONSOLE_TASK_PRIORITY 10

// Max queue sizes
#define MAX_SEND_QUEUE_SIZE 20

// Bookkeeping
//
int total_coins_in = 0;
int total_coins_out = 0;

// Queues
//
// static QueueHandle_t hopper_evt_queue = NULL;


static void console_task(void * arg)
{
    run_console();
}

static void game_task(void * arg)
{
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

        int payout = 0;

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
            
            // Wait for coin exit
            for (;;)
            {
                regs = REG_READ(GPIO_IN_REG);
                if  ((1ULL << GPIO_COIN_EXIT) & ~regs)
                {
                    // TODO: send message: coins in + 1 
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
    // Get global delay settings
    get_debounce(PAYOUT_DEBOUNCE);
    get_debounce(COIN_EXIT_DEBOUNCE);
    get_debounce(HOPPER_DEBOUNCE);

    // Connect to wifi (if we have the credentials stored in NVS)
    wifi_connect();

    // Initialize GPIO
    init_gpio();

    // Initialize display
    display_digit(0);

    // Initialize file system
    initialize_spiffs();

    // Create a queue to handle hopper event
    // hopper_evt_queue =  xQueueCreate(MAX_HOPPER_QUEUE_SIZE, sizeof(hopper_event));

    // start console task
    xTaskCreate(console_task, "Console Task", 10 * 1024, NULL, CONSOLE_TASK_PRIORITY, NULL);
    // start game task
    xTaskCreate(game_task, "Spar7 Game Task", 4096, NULL, CONSOLE_TASK_PRIORITY, NULL);

    while (true)
    {
        vTaskDelay(10 / portTICK_PERIOD_MS);
    }
}

