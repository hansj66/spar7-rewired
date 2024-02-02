#pragma once

#include <stdio.h>
#include <stdbool.h>
#include <stdint.h>

/**
 * Connect the WiFi adapter. When the wifi adapter is connected and receives an IP the COMMS_WIFI_ONLINE
 * event is raised.
 */
void wifi_connect();

