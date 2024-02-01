#ifndef _SETTINGS_SPAR7_H_
#define _SETTINGS_SPAR7_H_

#include <stddef.h>

#define SSID_MAX_LENGTH 50
#define PASSWORD_MAX_LENGTH 50

#define DEFAULT_PAYOUT_DELAY_MS     10
#define DEFAULT_COIN_EXIT_DELAY_MS  10
#define DEFAULT_HOPPER_DELAY_MS     300

typedef enum
{
    PAYOUT_DEBOUNCE = 0,
    COIN_EXIT_DEBOUNCE,
    HOPPER_DEBOUNCE,
} setting_t;

void set_debounce(setting_t s, int value);
int get_debounce(setting_t s);
void set_wifi(const char * ssid, const char * password);
void get_wifi_ssid(char * ssid, size_t* length);
void get_wifi_password(char * password, size_t* length);

#endif

