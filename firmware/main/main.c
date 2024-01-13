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

// CD4511BNSR BCD truth table {A,B,C,D}
//
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

// Task priorities (higher numbers == higher priorities)
//
#define GPIO_TASK_PRIORITY 10
#define PAYOUT_TASK_PRIORITY 10
#define EARNINGS_TASK_PRIORITY 10

// Max queue sizes
#define MAX_GPIO_QUEUE_SIZE 100
#define MAX_PAYOUT_QUEUE_SIZE 20
#define MAX_EARNINGS_QUEUE_SIZE 20
#define MAX_HOPPER_QUEUE_SIZE 100

// Motor RPM vs event timeout
#define MOTOR_RPM 60    
#define HOPPER_TIMEOUT_MS (1000*MOTOR_RPM / 60) / 4

const uint64_t debounce_limit_us = 250000;
uint64_t gpio_last_triggered_us[10] = {0,0,0,0,0,0,0,0,0,0};

// Output
//
const int GPIO_BCD_BASE = 8;
const int GPIO_BCD_A = GPIO_BCD_BASE;
const int GPIO_BCD_B = GPIO_BCD_BASE+1;
const int GPIO_BCD_C = GPIO_BCD_BASE+2;
const int GPIO_BCD_D = GPIO_BCD_BASE+3;
const int GPIO_RELAY = 12;

#define GPIO_OUTPUT_PIN_SEL  ((1ULL<<GPIO_BCD_BASE) | (1ULL<<GPIO_BCD_A) | (1ULL<<GPIO_BCD_B) | (1ULL<<GPIO_BCD_C) | (1ULL<<GPIO_BCD_D) | (1ULL<<GPIO_RELAY))

// Input
//
const int GPIO_EXT = 1;
const int GPIO_HOPPER = 2;
const int GPIO_COIN_EXIT = 3;
const int GPIO_PAY7 = 4;
const int GPIO_PAY4 = 5;
const int GPIO_PAY3 = 6;
const int GPIO_PAY2 = 7;

#define GPIO_INPUT_PIN_SEL  ((1ULL<<GPIO_EXT) | (1ULL<<GPIO_HOPPER) | (1ULL<<GPIO_COIN_EXIT) | (1ULL<<GPIO_PAY7) | (1ULL<<GPIO_PAY4) | (1ULL<<GPIO_PAY3) | (1ULL<<GPIO_PAY2))

#define ESP_INTR_FLAG_DEFAULT 0

// Bookkeeping
//
int total_coins_in = 0;
int total_coins_out = 0;

// Queues
//
static QueueHandle_t gpio_evt_queue = NULL;
static QueueHandle_t payout_evt_queue = NULL;
static QueueHandle_t earnings_evt_queue = NULL;
static QueueHandle_t hopper_evt_queue = NULL;

// Events
// 
struct _gpio_event 
{
    uint32_t gpio_num;
    int64_t timestamp_us;
} gpio_event;

struct _payout_event 
{
    uint8_t coins;
} payout_event;

struct _earnings_event 
{
    int8_t coins; // Negative for coins out. Positive for coins in.
} earnings_event;

uint8_t hopper_event = 1;


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

void start_motor()
{
    gpio_set_level(GPIO_RELAY, 1);
}

void stop_motor()
{
    gpio_set_level(GPIO_RELAY, 0);
}

void init_gpio()
{
    configure_gpio_input();
    configure_gpio_output();
}


static void IRAM_ATTR gpio_isr_handler(void* arg)
{
    struct _gpio_event e;
    e.gpio_num = (uint32_t) arg;

    struct timeval tv_now;
    gettimeofday(&tv_now, NULL);
    e.timestamp_us = (int64_t)tv_now.tv_sec * 1000000L + (int64_t)tv_now.tv_usec;

    xQueueSendFromISR(gpio_evt_queue, &e, NULL);
}



static void payout_task(void * arg)
{
    printf("Payout Task is running.\n");

    struct _payout_event e;
    for (;;) 
    {
        if (xQueueReceive(payout_evt_queue, &e, portMAX_DELAY)) 
        {
            printf("Handling payout of %d coins.\n", e.coins);
            start_motor();
            int payout = e.coins;
            display_digit(payout);
            uint8_t hopper_evt;
            printf("PAYOUT: Waiting for hopper event\n");
            for (int i=0; i<e.coins; i++)
            {
                // TODO: Add timeout
                BaseType_t ret = xQueueReceive(hopper_evt_queue, &hopper_evt,  pdMS_TO_TICKS(HOPPER_TIMEOUT_MS) /*portMAX_DELAY */);
                if (pdTRUE == ret)
                {
                    printf("PAYOUT: processing hopper event\n");
                    payout -= 1;
                    display_digit(payout);
                }
                else
                {
                    // TODO: add error routine (error codes == blanking + 8 + number + 8 + blank?)
                }
            }
            stop_motor();
        }
    }
}

static void earnings_task(void* arg)
{
    printf("Earnings Task is running.\n");

    struct _earnings_event e;
    for (;;) 
    {
        if (xQueueReceive(earnings_evt_queue, &e, portMAX_DELAY)) 
        {
            if (e.coins > 0)
                total_coins_in += e.coins;
            if (e.coins < 0)
                total_coins_out += abs(e.coins);

            // TODO: Pack into protobuffer and send to Span

            printf("Total Earnings : %d\n", total_coins_in - total_coins_out);
        }
    }
}

static void gpio_task(void* arg)
{
    printf("GPIO Task is running.\n");

    struct _gpio_event gpio_evt;
    struct _earnings_event earnings_evt;
    struct _payout_event payout_evt;
    for (;;) 
    {
        if (xQueueReceive(gpio_evt_queue, &gpio_evt, portMAX_DELAY)) 
        {
            if (gpio_evt.timestamp_us - gpio_last_triggered_us[gpio_evt.gpio_num] < debounce_limit_us)
            {
                continue;
            }
            if (0 == gpio_get_level(gpio_evt.gpio_num))
            {
                continue;
            }

            printf("GPIO[%"PRIu32"] intr, val: %d\n", gpio_evt.gpio_num, gpio_get_level(gpio_evt.gpio_num));
            gpio_last_triggered_us[gpio_evt.gpio_num] = gpio_evt.timestamp_us;

            switch (gpio_evt.gpio_num)
            {
                case GPIO_EXT:          break; // Nothing hooked up to this so far
                case GPIO_HOPPER:       printf("Hopper\n"); 
                                        xQueueSend(hopper_evt_queue, &hopper_event, 0);
                                        break;
                case GPIO_COIN_EXIT:    printf("Coin exit\n"); 
                                        earnings_evt.coins = 1;
                                        xQueueSend(earnings_evt_queue, &earnings_evt, 0);
                                        break;
                case GPIO_PAY7:         printf("Pay 7\n"); 
                                        earnings_evt.coins = -7;
                                        xQueueSend(earnings_evt_queue, &earnings_evt, 0);
                                        payout_evt.coins = 7;
                                        xQueueSend(payout_evt_queue, &payout_evt, 0);
                                        display_digit(7);
                                        break;
                case GPIO_PAY4:         printf("Pay 4\n"); 
                                        earnings_evt.coins = -4;
                                        xQueueSend(earnings_evt_queue, &earnings_evt, 0);
                                        payout_evt.coins = 4;
                                        xQueueSend(payout_evt_queue, &payout_evt, 0);
                                        display_digit(4);
                                        break;
                case GPIO_PAY3:         printf("Pay 3\n"); 
                                        earnings_evt.coins = -3;
                                        xQueueSend(earnings_evt_queue, &earnings_evt, 0);
                                        payout_evt.coins = 3;
                                        xQueueSend(payout_evt_queue, &payout_evt, 0);
                                        display_digit(3);
                                        break;
                case GPIO_PAY2:         printf("Pay 2\n"); 
                                        earnings_evt.coins = -2;
                                        xQueueSend(earnings_evt_queue, &earnings_evt, 0);
                                        payout_evt.coins = 2;
                                        xQueueSend(payout_evt_queue, &payout_evt, 0);
                                        display_digit(2);
                                        break;
            }
        }
    }
}


void print_welcome_message()
{
    vTaskDelay(3000 / portTICK_PERIOD_MS);

    printf("S7 rewired v.1.0 running.\n");
    printf("-------------------------\n");
    printf("Initial state:\n\n");
    printf("\tGPIO task priority:       %d\n", GPIO_TASK_PRIORITY);
    printf("\tPayout task priority:     %d\n", PAYOUT_TASK_PRIORITY);
    printf("\tEarnings task priority:   %d\n", EARNINGS_TASK_PRIORITY);
    printf("\n");
    printf("\t Max gpio queue size:     %d\n", MAX_GPIO_QUEUE_SIZE);
    printf("\t Max payout queue size:   %d\n", MAX_PAYOUT_QUEUE_SIZE);
    printf("\t Max earnings queue size: %d\n", MAX_EARNINGS_QUEUE_SIZE);
    printf("\n");
    printf("\tMotor RPM:                %d\n", MOTOR_RPM);
    printf("\tHopper timeout (ms):      %d\n", HOPPER_TIMEOUT_MS);
}


void app_main(void)
{
    print_welcome_message();

    // Initialize display
    display_digit(0);

    // Initialize file system
    initialize_spiffs();

    // Initialize GPIO
    init_gpio();

    // Create a queue to handle gpio event from isr
    gpio_evt_queue = xQueueCreate(MAX_GPIO_QUEUE_SIZE, sizeof(gpio_event));
    // Create a queue to handle payouts
    payout_evt_queue =  xQueueCreate(MAX_PAYOUT_QUEUE_SIZE, sizeof(payout_event));
    // Create a queue to handle earnings
    earnings_evt_queue =  xQueueCreate(MAX_EARNINGS_QUEUE_SIZE, sizeof(earnings_event));
    // Create a queue to handle hopper event
    hopper_evt_queue =  xQueueCreate(MAX_HOPPER_QUEUE_SIZE, sizeof(hopper_event));

    //start gpio task
    xTaskCreate(gpio_task, "GPIO Task", 4096, NULL, GPIO_TASK_PRIORITY, NULL);
    // start payout task
    xTaskCreate(payout_task, "Payout Task", 4096, NULL, PAYOUT_TASK_PRIORITY, NULL);
    // start earnings task
    xTaskCreate(earnings_task, "Earnings Task", 4096, NULL, EARNINGS_TASK_PRIORITY, NULL);

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

    while (true)
    {
        vTaskDelay(1000 / portTICK_PERIOD_MS);
    }
}
