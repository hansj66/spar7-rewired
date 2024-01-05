/*
 * SPDX-FileCopyrightText: 2010-2022 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: CC0-1.0
 */

#include <stdio.h>
#include <inttypes.h>
#include "sdkconfig.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_chip_info.h"
#include "esp_flash.h"
#include "esp_system.h"
#include "driver/gpio.h"

/*
    GPIO Output
*/
const int GPIO_BCD_A = 8;
const int GPIO_BCD_B = 9;
const int GPIO_BCD_C = 10;
const int GPIO_BCD_D = 11;
const int GPIO_RELAY = 12;

/*
    GPIO Input
*/
const int GPIO_EXT = 1;
const int GPIO_HOPPER = 2;
const int GPIO_COIN_EXIT = 3;
const int GPIO_PAY7 = 4;
const int GPIO_PAY4 = 5;
const int GPIO_PAY3 = 6;
const int GPIO_PAY2 = 7;


/*
TODO: Neste versjon

Strapping pin: GPIO0, GPIO2, GPIO5, GPIO12 (MTDI), and GPIO15 (MTDO) are strapping pins. For more infomation, please refer to ESP32 datasheet.

SPI0/1: GPIO6-11 and GPIO16-17 are usually connected to the SPI flash and PSRAM integrated on the module and therefore should not be used for other purposes.

JTAG: GPIO12-15 are usually used for inline debug.

GPI: GPIO34-39 can only be set as input mode and do not have software-enabled pullup or pulldown functions.

TXD & RXD are usually used for flashing and debugging.

ADC2: ADC2 pins cannot be used when Wi-Fi is used. So, if you are having trouble getting the value from an ADC2 GPIO while using Wi-Fi, you may consider using an ADC1 GPIO instead, which should solve your problem. For more details, please refer to Hardware Limitations of ADC Continuous Mode and Hardware Limitations of ADC Oneshot Mode.

Please do not use the interrupt of GPIO36 and GPIO39 when using ADC or Wi-Fi and Bluetooth with sleep mode enabled. Please refer to ESP32 ECO and Workarounds for Bugs > Section 3.11 for the detailed description of the issue.

app_main() {
	gpio_config_t io_conf;
	io_conf.intr_type = GPIO_INTR_DISABLE;
	io_conf.mode = GPIO_MODE_OUTPUT;
	io_conf.pin_bit_mask = GPIO_BIT_MASK;
	io_conf.pull_down_en = GPIO_PULLDOWN_DISABLE;
	io_conf.pull_up_en = GPIO_PULLUP_DISABLE;
	gpio_config(&io_conf);
}

*/

/*
    IO1     EXT
    IO2     HOPPER
    IO3     COIN_EXIT
    IO4     PAY7
    IO5     PAY4
    IO6     PAY3
    IO7     PAY2
    IO8     BCD A
    IO9     BCD B
    IO10    BCD C
    IO11    BCD D
    IO12    RELAY


    CD4511BNSR truth table:

    DCBA
    0000    0
    0001    1
    0010    2
    0011    3
    0100    4
    0101    5
    0110    6
    0111    7
    1000    8
    1001    9
    1111    blank

    (inverted through ULN2003A)

*/ 

void init_gpio()
{

    // Output
    gpio_set_direction(GPIO_BCD_A, GPIO_MODE_OUTPUT);
    gpio_set_direction(GPIO_BCD_B, GPIO_MODE_OUTPUT);
    gpio_set_direction(GPIO_BCD_C, GPIO_MODE_OUTPUT);
    gpio_set_direction(GPIO_BCD_D, GPIO_MODE_OUTPUT);
    gpio_set_direction(GPIO_RELAY, GPIO_MODE_OUTPUT);

    // Input

    gpio_set_direction(GPIO_EXT, GPIO_MODE_INPUT);
    gpio_set_direction(GPIO_HOPPER, GPIO_MODE_INPUT);
    gpio_set_direction(GPIO_COIN_EXIT, GPIO_MODE_INPUT);
    gpio_set_direction(GPIO_PAY7, GPIO_MODE_INPUT);
    gpio_set_direction(GPIO_PAY4, GPIO_MODE_INPUT);
    gpio_set_direction(GPIO_PAY3, GPIO_MODE_INPUT);
    gpio_set_direction(GPIO_PAY2, GPIO_MODE_INPUT);

}


void app_main(void)
{
    init_gpio();

//    DCBA
//    0000    0

    gpio_set_level(GPIO_BCD_A, 0);
    gpio_set_level(GPIO_BCD_B, 0);
    gpio_set_level(GPIO_BCD_C, 0);
    gpio_set_level(GPIO_BCD_D, 0);

    while (true)
    {
        printf("S7 rewired\n");
        gpio_set_level(GPIO_BCD_A, 0);
        gpio_set_level(GPIO_BCD_B, 0);
        gpio_set_level(GPIO_BCD_C, 0);
        gpio_set_level(GPIO_BCD_D, 0);
        gpio_set_level(GPIO_RELAY, 0);

        vTaskDelay(1000 / portTICK_PERIOD_MS);
        gpio_set_level(GPIO_BCD_A, 1);
        gpio_set_level(GPIO_BCD_B, 1);
        gpio_set_level(GPIO_BCD_C, 1);
        gpio_set_level(GPIO_RELAY, 1);

        vTaskDelay(1000 / portTICK_PERIOD_MS);
    }
}
