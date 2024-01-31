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
#include "driver/dedic_gpio.h"
#include "sys/time.h"
#include "filesystem.h"
#include "esp_log.h"
#include "soc/soc.h"
#include "soc/gpio_reg.h"


// ----------------------------------------------------------------------------------------------------
// Lesestoff: https://www.eevblog.com/forum/projects/long-wires-connecting-a-switch-to-a-processor-pin/
// Mål frekvensen på støyen ?

static const char* LOGTAG = "SPAR7";

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

const uint64_t debounce_limit_us = 200000;
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
    // gpio_config_t io_conf = {};
    // io_conf.intr_type = GPIO_INTR_NEGEDGE;
    // io_conf.pin_bit_mask = GPIO_INPUT_PIN_SEL;
    // io_conf.mode = GPIO_MODE_INPUT;
    // io_conf.pull_down_en = 0;
    // io_conf.pull_up_en = 1;
    gpio_config_t io_conf = {};
    io_conf.intr_type = GPIO_INTR_DISABLE;
    io_conf.pin_bit_mask = GPIO_INPUT_PIN_SEL;
    io_conf.mode = GPIO_MODE_INPUT;
    io_conf.pull_down_en = 0;
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


// static void IRAM_ATTR gpio_isr_handler(void* arg)
// {
//     struct _gpio_event e;
//     e.gpio_num = (uint32_t) arg;

//     struct timeval tv_now;
//     gettimeofday(&tv_now, NULL);
//     e.timestamp_us = (int64_t)tv_now.tv_sec * 1000000L + (int64_t)tv_now.tv_usec;

//     xQueueSendFromISR(gpio_evt_queue, &e, NULL);
// }



static void payout_task(void * arg)
{
    ESP_LOGI(LOGTAG, "Payout Task is running.");

    struct _payout_event e;
    for (;;) 
    {
        if (xQueueReceive(payout_evt_queue, &e, portMAX_DELAY)) 
        {
            ESP_LOGI(LOGTAG, "Handling payout of %d coins.", e.coins);
            int payout = e.coins;
            display_digit(payout);
            vTaskDelay(300 / portTICK_PERIOD_MS);
            start_motor();
            uint8_t hopper_evt;
            ESP_LOGI(LOGTAG, "Waiting for hopper event.");
            for (int i=0; i<e.coins; i++)
            {
                // TODO: Add timeout
                // BaseType_t ret = xQueueReceive(hopper_evt_queue, &hopper_evt,  pdMS_TO_TICKS(HOPPER_TIMEOUT_MS) /*portMAX_DELAY */);
                BaseType_t ret = xQueueReceive(hopper_evt_queue, &hopper_evt,  portMAX_DELAY);
                if (pdTRUE == ret)
                {
                    ESP_LOGI(LOGTAG, "processing hopper event.");
                    payout -= 1;
                    display_digit(payout);
                }
                else
                {
                    ESP_LOGW(LOGTAG, "Hopper timeout.");
                }
            }
            vTaskDelay(30 / portTICK_PERIOD_MS);
            stop_motor();
        }
    }
}

static void earnings_task(void* arg)
{
    ESP_LOGI(LOGTAG, "Earnings Task is running.");

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

            ESP_LOGI(LOGTAG, "Total Earnings : %d.", total_coins_in - total_coins_out);
        }
    }
}

static void gpio_task(void* arg)
{
    ESP_LOGI(LOGTAG, "GPIO Task is running.");

    struct _gpio_event gpio_evt;
    struct _earnings_event earnings_evt;
    struct _payout_event payout_evt;
    for (;;) 
    {
        if (pdTRUE == xQueueReceive(gpio_evt_queue, &gpio_evt, portMAX_DELAY)) 
        {
            // if (gpio_evt.timestamp_us - gpio_last_triggered_us[gpio_evt.gpio_num] > debounce_limit_us) /*&& (0 != gpio_get_level(gpio_evt.gpio_num)*/
            // {
                // ESP_LOGI(LOGTAG, "GPIO[%"PRIu32"] intr, val: %d, t: [%"PRIu64"], l:[%"PRIu64"], b:[%"PRIu64"]", gpio_evt.gpio_num, gpio_get_level(gpio_evt.gpio_num), gpio_evt.timestamp_us, gpio_last_triggered_us[gpio_evt.gpio_num], gpio_evt.timestamp_us - gpio_last_triggered_us[gpio_evt.gpio_num]);
                ESP_LOGI(LOGTAG, "GPIO[%"PRIu32"] intr, val: %d, t: [%"PRIu64"], l:[%"PRIu64"], b:[%"PRIu64"]", gpio_evt.gpio_num, gpio_get_level(gpio_evt.gpio_num), gpio_evt.timestamp_us, gpio_last_triggered_us[gpio_evt.gpio_num], gpio_evt.timestamp_us - gpio_last_triggered_us[gpio_evt.gpio_num]);
                // gpio_last_triggered_us[gpio_evt.gpio_num] = gpio_evt.timestamp_us;

                switch (gpio_evt.gpio_num)
                {
                    case GPIO_EXT:          break; // Nothing hooked up to this so far
                    case GPIO_HOPPER:       xQueueSend(hopper_evt_queue, &hopper_event, 0);
                                            break;
                    case GPIO_COIN_EXIT:    earnings_evt.coins = 1;
                                            xQueueSend(earnings_evt_queue, &earnings_evt, 0);
                                            break;
                    case GPIO_PAY7:         earnings_evt.coins = -7;
                                            xQueueSend(earnings_evt_queue, &earnings_evt, 0);
                                            payout_evt.coins = 7;
                                            xQueueSend(payout_evt_queue, &payout_evt, 0);
                                            display_digit(7);
                                            break;
                    case GPIO_PAY4:         earnings_evt.coins = -4;
                                            xQueueSend(earnings_evt_queue, &earnings_evt, 0);
                                            payout_evt.coins = 4;
                                            xQueueSend(payout_evt_queue, &payout_evt, 0);
                                            display_digit(4);
                                            break;
                    case GPIO_PAY3:         earnings_evt.coins = -3;
                                            xQueueSend(earnings_evt_queue, &earnings_evt, 0);
                                            payout_evt.coins = 3;
                                            xQueueSend(payout_evt_queue, &payout_evt, 0);
                                            display_digit(3);
                                            break;
                    case GPIO_PAY2:         earnings_evt.coins = -2;
                                            xQueueSend(earnings_evt_queue, &earnings_evt, 0);
                                            payout_evt.coins = 2;
                                            xQueueSend(payout_evt_queue, &payout_evt, 0);
                                            display_digit(2);
                                            break;
                // }
            }
        }
    }
}


void print_welcome_message()
{
    vTaskDelay(3000 / portTICK_PERIOD_MS);

    ESP_LOGI(LOGTAG, "S7 rewired v.1.0 running.");
    ESP_LOGI(LOGTAG, "-------------------------");
    ESP_LOGI(LOGTAG, "Initial state:\n\n");
    ESP_LOGI(LOGTAG, "\tGPIO task priority:       %d", GPIO_TASK_PRIORITY);
    ESP_LOGI(LOGTAG, "\tPayout task priority:     %d", PAYOUT_TASK_PRIORITY);
    ESP_LOGI(LOGTAG, "\tEarnings task priority:   %d", EARNINGS_TASK_PRIORITY);
    ESP_LOGI(LOGTAG, " ");
    ESP_LOGI(LOGTAG, "\t Max gpio queue size:     %d", MAX_GPIO_QUEUE_SIZE);
    ESP_LOGI(LOGTAG, "\t Max payout queue size:   %d", MAX_PAYOUT_QUEUE_SIZE);
    ESP_LOGI(LOGTAG, "\t Max earnings queue size: %d", MAX_EARNINGS_QUEUE_SIZE);
    ESP_LOGI(LOGTAG, " ");
    ESP_LOGI(LOGTAG, "\tMotor RPM:                %d", MOTOR_RPM);
    ESP_LOGI(LOGTAG, "\tHopper timeout (ms):      %d", HOPPER_TIMEOUT_MS);
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

    // //start gpio task
    // xTaskCreate(gpio_task, "GPIO Task", 4096, NULL, GPIO_TASK_PRIORITY, NULL);
    // // start payout task
    // xTaskCreate(payout_task, "Payout Task", 4096, NULL, PAYOUT_TASK_PRIORITY, NULL);
    // // start earnings task
    // xTaskCreate(earnings_task, "Earnings Task", 4096, NULL, EARNINGS_TASK_PRIORITY, NULL);

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
                    // Increment earnings by 1
                    printf("Coin exit\n");
                    break;
                }
                vTaskDelay(10 / portTICK_PERIOD_MS);
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

                        // Only debounce first n-1 couins
                        if (countdown >= 1)
                        {
                            vTaskDelay(140 / portTICK_PERIOD_MS);
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
        vTaskDelay(10 / portTICK_PERIOD_MS);
    }
}

