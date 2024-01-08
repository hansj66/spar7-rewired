#include <stdio.h>
#include <inttypes.h>
#include "sdkconfig.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/task.h"
#include "esp_chip_info.h"
#include "esp_flash.h"
#include "esp_system.h"
#include "driver/gpio.h"
#include "sys/time.h"
#include "filesystem.h"

/*
    CD4511BNSR BCD truth table {A,B,C,D}
*/
int BCD_matrix[11][4] = {
    {0, 0, 0, 0}, // 0
    {1, 0, 0, 0}, // 1
    {0, 1, 0, 0}, // 2
    {1, 1, 0, 0}, // 3
    {0, 0, 1, 0}, // 4
    {1, 0, 1, 0}, // 5
    {0, 1, 1, 0}, // 6
    {1, 1, 1, 0}, // 7
    {0, 0, 0, 1}, // 8
    {1, 0, 0, 1}, // 9
    {1, 1, 1, 1}, // blank
};

const uint64_t debounce_limit_us = 1000000;
uint64_t gpio_last_triggered_us[10] = {0,0,0,0,0,0,0,0,0,0};


/*
    Output
*/
const int GPIO_BCD_BASE = 8;
const int GPIO_BCD_A = GPIO_BCD_BASE;
const int GPIO_BCD_B = GPIO_BCD_BASE+1;
const int GPIO_BCD_C = GPIO_BCD_BASE+2;
const int GPIO_BCD_D = GPIO_BCD_BASE+3;
const int GPIO_RELAY = 12;

#define GPIO_OUTPUT_PIN_SEL  ((1ULL<<GPIO_BCD_BASE) | (1ULL<<GPIO_BCD_A) | (1ULL<<GPIO_BCD_B) | (1ULL<<GPIO_BCD_C) | (1ULL<<GPIO_BCD_D) | (1ULL<<GPIO_RELAY))

/*
    Input
*/
const int GPIO_EXT = 1;
const int GPIO_HOPPER = 2;
const int GPIO_COIN_EXIT = 3;
const int GPIO_PAY7 = 4;
const int GPIO_PAY4 = 5;
const int GPIO_PAY3 = 6;
const int GPIO_PAY2 = 7;

#define GPIO_INPUT_PIN_SEL  ((1ULL<<GPIO_EXT) | (1ULL<<GPIO_HOPPER) | (1ULL<<GPIO_COIN_EXIT) | (1ULL<<GPIO_PAY7) | (1ULL<<GPIO_PAY4) | (1ULL<<GPIO_PAY3) | (1ULL<<GPIO_PAY2))

#define ESP_INTR_FLAG_DEFAULT 0

static QueueHandle_t gpio_evt_queue = NULL;

struct _gpio_event 
{
    uint32_t gpio_num;
    int64_t timestamp_us;
} gpio_event;

int payout = 0;

static void IRAM_ATTR gpio_isr_handler(void* arg)
{
    struct _gpio_event e;
    e.gpio_num = (uint32_t) arg;

    struct timeval tv_now;
    gettimeofday(&tv_now, NULL);
    e.timestamp_us = (int64_t)tv_now.tv_sec * 1000000L + (int64_t)tv_now.tv_usec;

    xQueueSendFromISR(gpio_evt_queue, &e, NULL);
}

static void gpio_task(void* arg)
{
    printf("GPIO Task is running.\n");
    struct _gpio_event e;
    for (;;) 
    {
        if (xQueueReceive(gpio_evt_queue, &e, portMAX_DELAY)) 
        {
            if (e.timestamp_us - gpio_last_triggered_us[e.gpio_num] > debounce_limit_us)
            {
                printf("GPIO[%"PRIu32"] intr, val: %d\n", e.gpio_num, gpio_get_level(e.gpio_num));
                gpio_last_triggered_us[e.gpio_num] = e.timestamp_us;

                switch (e.gpio_num)
                {
                    case GPIO_EXT: printf("EXT\n"); break;
                    case GPIO_HOPPER: printf("Hopper\n"); break;
                    case GPIO_COIN_EXIT: printf("Coin exit\n"); break;
                    case GPIO_PAY7: printf("Pay 7\n"); break;
                    case GPIO_PAY4: printf("Pay 4\n"); break;
                    case GPIO_PAY3: printf("Pay 3\n"); break;
                    case GPIO_PAY2: printf("Pay 2\n"); break;
                }
            }
            
        }
    }
}

void configure_gpio_output() 
{
    gpio_config_t io_conf = {};
    io_conf.intr_type = GPIO_INTR_DISABLE;
    io_conf.mode = GPIO_MODE_OUTPUT;
    io_conf.pin_bit_mask = GPIO_OUTPUT_PIN_SEL;
    io_conf.pull_down_en = 0;
    io_conf.pull_up_en = 0;
    gpio_config(&io_conf);    
}

void configure_gpio_input()
{
    gpio_config_t io_conf = {};
    io_conf.intr_type = GPIO_INTR_POSEDGE;
    io_conf.pin_bit_mask = GPIO_INPUT_PIN_SEL;
    io_conf.mode = GPIO_MODE_INPUT;
    io_conf.pull_up_en = 1;
    gpio_config(&io_conf);
}

void display_digit(int digit)
{
    for (int i=0; i<4; i++)
    {
        gpio_set_level(GPIO_BCD_BASE+i, !BCD_matrix[digit][i]);
    }
}

void init_gpio()
{
    configure_gpio_input();
    configure_gpio_output();
}

void print_welcome_message()
{
    printf("S7 rewired v.1.0 running.\n");
    printf("-------------------------\n");
}


void app_main(void)
{
    // Initialize file system
    initialize_spiffs();

    // Initialize GPIO
    init_gpio();
    //create a queue to handle gpio event from isr
    gpio_evt_queue = xQueueCreate(10, sizeof(gpio_event));
    //start gpio task
    xTaskCreate(gpio_task, "gpio_task", 2048, NULL, 10, NULL);

    //install gpio isr service
    gpio_install_isr_service(ESP_INTR_FLAG_DEFAULT);
    //hook isr handler for gpio pins
    gpio_isr_handler_add(GPIO_EXT, gpio_isr_handler, (void*) GPIO_EXT);
    gpio_isr_handler_add(GPIO_HOPPER, gpio_isr_handler, (void*) GPIO_HOPPER);
    gpio_isr_handler_add(GPIO_COIN_EXIT, gpio_isr_handler, (void*) GPIO_COIN_EXIT);
    gpio_isr_handler_add(GPIO_PAY7, gpio_isr_handler, (void*) GPIO_PAY7);
    gpio_isr_handler_add(GPIO_PAY4, gpio_isr_handler, (void*) GPIO_PAY4);
    gpio_isr_handler_add(GPIO_PAY3, gpio_isr_handler, (void*) GPIO_PAY3);
    gpio_isr_handler_add(GPIO_PAY2, gpio_isr_handler, (void*) GPIO_PAY2);

    print_welcome_message();


    int i=0;
    while (true)
    {
        printf("\n");

        display_digit(i);
        i++;
        if (i > 10)
            i = 0;
       //  gpio_set_level(GPIO_RELAY, 0);

        vTaskDelay(1000 / portTICK_PERIOD_MS);

//        vTaskDelay(1000 / portTICK_PERIOD_MS);
    }
}
