#pragma once
#include "esp_err.h"

/**
 * These constants will have to be kept in sync with the protobuffer defintions
 * (there are, of course no easy way to keep these in sync without parsing the .proto or generated pb.h files)
 */
#define APN_NAME_MAX_LENGTH 20
#define COAP_ADDRESS_MAX_LENGTH 30
#define SSID_MAX_LENGTH 30
#define WIFI_PASSWORD_MAX_LENGTH 30
#define ADDR_MAX_LENGTH 16
/**
 * Configuration for communication library
 */
typedef struct
{
    char apn_name[APN_NAME_MAX_LENGTH];      // (APN) Name of APN to use
    char coap_address[ADDR_MAX_LENGTH];      // (APN) COAP Address
    int coap_port;                           // (APN) COAP Port
    char ssid[SSID_MAX_LENGTH];              // (WiFi) SSID for WiFi
    char password[WIFI_PASSWORD_MAX_LENGTH]; // (WiFi) Password for WiFi
} comms_config_t;

/**
 * Returns a ponter to the current communication interface configuration
 */
comms_config_t *comms_get_config();

/**
 * Initializes comms
 */
comms_config_t *comms_config_init();
/**
 * Write new configuration for comms. This is stored in NVM (spiffs FS or elsewhere)
 */
esp_err_t comms_write_config(comms_config_t *config);

/**
 * Read stored configuration for comms. This is stored in NVM (spiffs FS). Returns an error if there is
 * no stored configuration.
 */
esp_err_t comms_read_config(comms_config_t *config);

/**
 * Set the default config for the comms
 */
void comms_default_config(comms_config_t *config);