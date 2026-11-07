#pragma once

// Default values; the settings page stores changes in EEPROM, which take precedence.
#define WIFI_SSID "RajeshJioFiber"
#define WIFI_PASSWORD "Rajesh@1998"
#define WIFI_CONNECT_TIMEOUT_MS 15000
#define WIFI_CHECK_INTERVAL_MS 600000UL

// Hotspot used when the station connection fails; password must be 8+ characters.
#define AP_SSID "TempleLights"
#define AP_PASSWORD "123456789"

// Credentials asked by the /update firmware upload page; change before deploying.
#define OTA_USER "admin"
#define OTA_PASSWORD "admin1234"
