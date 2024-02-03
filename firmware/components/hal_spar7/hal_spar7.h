#ifndef _HAL_SPAR7_H_
#define _HAL_SPAR7_H_

// Output
//
#define GPIO_BCD_BASE 8
#define GPIO_BCD_A 8
#define GPIO_BCD_B 9
#define GPIO_BCD_C 10
#define GPIO_BCD_D 11
#define GPIO_RELAY 12

#define GPIO_OUTPUT_PIN_SEL  ((1ULL<<GPIO_BCD_BASE) | (1ULL<<GPIO_BCD_A) | (1ULL<<GPIO_BCD_B) | (1ULL<<GPIO_BCD_C) | (1ULL<<GPIO_BCD_D) | (1ULL<<GPIO_RELAY))

// Input
//
#define GPIO_EXT 1
#define GPIO_HOPPER 2
#define GPIO_COIN_EXIT 3
#define GPIO_PAY7 4
#define GPIO_PAY4 5
#define GPIO_PAY3 6
#define GPIO_PAY2 7

#define GPIO_INPUT_PIN_SEL  ((1ULL<<GPIO_EXT) | (1ULL<<GPIO_HOPPER) | (1ULL<<GPIO_COIN_EXIT) | (1ULL<<GPIO_PAY7) | (1ULL<<GPIO_PAY4) | (1ULL<<GPIO_PAY3) | (1ULL<<GPIO_PAY2))

void display_digit(int digit);
void start_motor();
void stop_motor();
void init_gpio();


#endif