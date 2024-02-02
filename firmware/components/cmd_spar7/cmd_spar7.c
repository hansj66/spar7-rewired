#include "esp_log.h"
#include "esp_console.h"
#include "argtable3/argtable3.h"
#include "cmd_spar7.h"
#include "settings_spar7.h"
#include <string.h>
#include "dtls.h"

static void register_config_debounce(void);
static void register_debounce_info(void);
static void register_config_wifi(void);
static void register_wifi_info(void);
static void register_tx_test(void);


static const char *TAG = "cmd_spar7";

void register_spar7(void)
{
    register_config_debounce();
    register_debounce_info();
    register_config_wifi();
    register_wifi_info();
    register_tx_test();
}

static struct {
    struct arg_str *group;
    struct arg_str *delay;
    struct arg_end *end;
} debounce_args;

static struct {
    struct arg_str *ssid;
    struct arg_str *password;
    struct arg_end *end;
} wifi_args;


static int debounce_delay(int argc, char **argv)
{
    int nerrors = arg_parse(argc, argv, (void **) &debounce_args);
    if (nerrors != 0) {
        arg_print_errors(stderr, debounce_args.end, argv[0]);
        return 1;
    }
    assert(debounce_args.group->count == 1);
    assert(debounce_args.delay->count == 1);

    const char* group = debounce_args.group->sval[0];
    int debounce_ms = atoi(debounce_args.delay->sval[0]);

    if (0 == strcmp(group, "hopper"))
    {
        set_debounce(HOPPER_DEBOUNCE, debounce_ms);
        return 0;
    }

    if (0 == strcmp(group, "exit"))
    {
        set_debounce(COIN_EXIT_DEBOUNCE, debounce_ms);
        return 0;
    }

    if (0 == strcmp(group, "payout"))
    {
        set_debounce(PAYOUT_DEBOUNCE, debounce_ms);
        return 0;
    }

    return 1;
}



static void register_config_debounce(void)
{
    debounce_args.group = arg_str1(NULL, NULL, "<hopper|exit|payout>", "Switch type to define debounce limit for");
    debounce_args.delay = arg_str1(NULL, NULL, "<ms>", "Debounce delay in milliseconds");
    debounce_args.end = arg_end(2);

    const esp_console_cmd_t cmd = {
        .command = "set_delay",
        .help = "Set debounce delay for a switch group.",
        .hint = NULL,
        .func = &debounce_delay,
        .argtable = &debounce_args
    };
    ESP_ERROR_CHECK( esp_console_cmd_register(&cmd) );
}

static int debounce_info(int argc, char **argv)
{
    printf("Payout switches delay: %d ms\n", get_debounce(PAYOUT_DEBOUNCE));
    printf("Coin exit switchdelay: %d ms\n", get_debounce(COIN_EXIT_DEBOUNCE));
    printf("Hopper switch delay:   %d ms\n", get_debounce(HOPPER_DEBOUNCE));
    return 0;
}


static void register_debounce_info(void)
{
    const esp_console_cmd_t cmd = {
        .command = "get_delays",
        .help = "Get information about current debounce / delay settings",
        .hint = NULL,
        .func = &debounce_info,
    };
    ESP_ERROR_CHECK( esp_console_cmd_register(&cmd) );
}

static int set_wifi_settings(int argc, char **argv)
{
    int nerrors = arg_parse(argc, argv, (void **) &wifi_args);
    if (nerrors != 0) {
        arg_print_errors(stderr, debounce_args.end, argv[0]);
        return 1;
    }
    assert(wifi_args.ssid->count == 1);
    assert(wifi_args.password->count == 1);

    set_wifi(wifi_args.ssid->sval[0], wifi_args.password->sval[0]);
    return 0;
}

static void register_config_wifi(void)
{
    wifi_args.ssid = arg_str1(NULL, NULL, "<ssid>", "Wifi ssid");
    wifi_args.password = arg_str1(NULL, NULL, "<password>", "Wifi password");
    wifi_args.end = arg_end(2);

    const esp_console_cmd_t cmd = {
        .command = "set_wifi",
        .help = "Set wifi ssid/password for your local network. (required for backend functionality)",
        .hint = NULL,
        .func = &set_wifi_settings,
        .argtable = &wifi_args
    };
    ESP_ERROR_CHECK( esp_console_cmd_register(&cmd) );
}


static int wifi_info(int argc, char **argv)
{
    char tmpBuf[50];
    size_t length = sizeof(tmpBuf);
    memset(tmpBuf, 0, length);
    get_wifi_ssid(tmpBuf, &length);
    printf("SSID: %s\n", tmpBuf);
    length = sizeof(tmpBuf);
    memset(tmpBuf, 0, length);
    get_wifi_password(tmpBuf, &length);
    printf("password: %s\n", tmpBuf);
    return 0;
}


static void register_wifi_info()
{
     const esp_console_cmd_t cmd = {
        .command = "get_wifi",
        .help = "Get wifi settings",
        .hint = NULL,
        .func = &wifi_info,
    };
    ESP_ERROR_CHECK( esp_console_cmd_register(&cmd) );
}

#define MESSAGE "Toodledoo!"
#define HOST "data.lab5e.com"
#define PORT "1234"

static int tx_test(int argc, char **argv)
{
    dtls_state_t dtls;

    if (!dtls_connect(&dtls, HOST, PORT)) 
    {
        dtls_close(&dtls);
        return 2;
    }
  
    printf("Connected to %s:%s\n", HOST, PORT);

    if (!dtls_send(&dtls, MESSAGE, strlen(MESSAGE))) 
    {
        dtls_close(&dtls);
        return 3;
    }

    printf("Sent message\n");

    dtls_close(&dtls);
    printf("Closed connection\n");
    return 0;
}



static void register_tx_test()
{
     const esp_console_cmd_t cmd = {
        .command = "txtest",
        .help = "Send test packet to endpoint",
        .hint = NULL,
        .func = &tx_test,
    };
    ESP_ERROR_CHECK( esp_console_cmd_register(&cmd) );
}


