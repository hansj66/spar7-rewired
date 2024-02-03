#include "hal_spar7.h"

#include "driver/gpio.h"
#include "driver/dedic_gpio.h"


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

