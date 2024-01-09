#pragma once
#include <stdint.h>
#include "esp_event.h"
/**
 * Events and event loop handling for the comms library
 */

ESP_EVENT_DECLARE_BASE(COMMS_EVENT);

typedef enum
{
    COMMS_WIFI_ONLINE,  // WiFi is online and has IP, wifi info parameters
    COMMS_WIFI_OFFLINE, // WiFi is offline
    COMMS_WIFI_ERROR,   // WiFi has had an error connecting
    COMMS_LTE_ONLINE,   // LTE is online has IP, LTE info in parameters
    COMMS_LTE_OFFLINE,  // LTE library is offline
    COMMS_LTE_ERROR,    // LTE connect error
    COMMS_ERROR,        // Comms is unavailable
    COMMS_CLOCK,        // Clock is set and system is ready
} comms_event_t;

#define IMSI_MAX_LENGTH 16
#define IMEI_MAX_LENGTH 16
#define IP_MAX_LENGTH 16
#define MODEM_VERSION_MAX_LENGTH 31
#define TIMESTRING_MAX_LENGTH 64
/**
 * Event parameter for COMMS_WIFI_ONLINE
 */
typedef struct
{
    char wifi_ip[IP_MAX_LENGTH]; // (WiFi) Allocated IP
} wifi_info_t;

/**
 * Event parameter for COMMS_LTE_ONLINE
 */
typedef struct
{
    char imsi[IMSI_MAX_LENGTH];                   // IMSI
    char imei[IMEI_MAX_LENGTH];                   // IMEI
    char lte_ip[IP_MAX_LENGTH];                   // Allocated IP
    char modem_version[MODEM_VERSION_MAX_LENGTH]; // Firmware version for modem
    char timestring[TIMESTRING_MAX_LENGTH];       // Time string
} lte_info_t;

/**
 * Register event handler for comms events
 */
void comms_register_event_handler(esp_event_handler_t event_handler, void *handler_args);

/**
 * Run the event pump once for the comms events
 */
void comms_event_pump();

/**
 * Init the comms event handler pump. Internal use in comms library only.
 */
esp_err_t comms_event_handler_init();

/**
 * Generate a comms event. Internal use in comms library only.
 */
void comms_event_post(comms_event_t evt, void *evt_data, size_t evt_data_size);