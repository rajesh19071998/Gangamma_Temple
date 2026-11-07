#pragma once

#include <Arduino.h>
#include <EEPROM.h>
#include <stddef.h>

#include "wifi_config.h"

// Settings persisted in the ESP8266 flash-backed EEPROM; wifi_config.h values are the defaults.
struct Settings {
    uint32_t magic;
    char wifiSsid[33];
    char wifiPass[64];
    char apSsid[33];
    char apPass[64];
    char otaUser[32];
    char otaPass[32];
    uint32_t checksum;
};

constexpr uint32_t SETTINGS_MAGIC = 0x54454D50;  // "TEMP"

static Settings settings;

inline uint32_t Settings_Checksum(const Settings &s)
{
    const uint8_t *p = reinterpret_cast<const uint8_t *>(&s);
    uint32_t hash = 2166136261UL;  // FNV-1a
    for (size_t i = 0; i < offsetof(Settings, checksum); i++) {
        hash = (hash ^ p[i]) * 16777619UL;
    }
    return hash;
}

inline void Settings_Copy(char *dst, size_t size, const char *src)
{
    strncpy(dst, src, size - 1);
    dst[size - 1] = '\0';
}

inline void Settings_LoadDefaults()
{
    memset(&settings, 0, sizeof(settings));
    settings.magic = SETTINGS_MAGIC;
    Settings_Copy(settings.wifiSsid, sizeof(settings.wifiSsid), WIFI_SSID);
    Settings_Copy(settings.wifiPass, sizeof(settings.wifiPass), WIFI_PASSWORD);
    Settings_Copy(settings.apSsid, sizeof(settings.apSsid), AP_SSID);
    Settings_Copy(settings.apPass, sizeof(settings.apPass), AP_PASSWORD);
    Settings_Copy(settings.otaUser, sizeof(settings.otaUser), OTA_USER);
    Settings_Copy(settings.otaPass, sizeof(settings.otaPass), OTA_PASSWORD);
}

inline void Settings_Load()
{
    EEPROM.begin(sizeof(Settings));
    EEPROM.get(0, settings);

    bool valid = settings.magic == SETTINGS_MAGIC && settings.checksum == Settings_Checksum(settings);
    if (valid) {
        // Guard against unterminated strings from a corrupted-but-matching record.
        settings.wifiSsid[sizeof(settings.wifiSsid) - 1] = '\0';
        settings.wifiPass[sizeof(settings.wifiPass) - 1] = '\0';
        settings.apSsid[sizeof(settings.apSsid) - 1] = '\0';
        settings.apPass[sizeof(settings.apPass) - 1] = '\0';
        settings.otaUser[sizeof(settings.otaUser) - 1] = '\0';
        settings.otaPass[sizeof(settings.otaPass) - 1] = '\0';
    } else {
        Settings_LoadDefaults();
    }
}

inline bool Settings_Save()
{
    settings.magic = SETTINGS_MAGIC;
    settings.checksum = Settings_Checksum(settings);
    EEPROM.put(0, settings);
    return EEPROM.commit();
}
