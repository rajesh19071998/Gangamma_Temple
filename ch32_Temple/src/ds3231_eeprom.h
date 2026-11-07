#ifndef DS3231_EEPROM_H
#define DS3231_EEPROM_H

#include <stddef.h>
#include <ch32v00x.h>

#include "ds3231.h"

/*
 * Persistent storage for the light/switch snapshot and daily alarm settings.
 * The DS3231 has no user memory (registers end at 0x12), so this uses the AT24C32
 * EEPROM found on DS3231 modules (I2C address 0x57, 16-bit memory address).
 */

#define DS3231_EEPROM_I2C_ADDRESS 0x57
#define DS3231_EEPROM_WRITE_POLL_TRIES 20U

#define DS3231_STATUS_MAGIC_VALUE 0xA5
#define DS3231_ALARM_MAGIC_VALUE 0x5A

/* Status block: magic, lights, switches. */
#define DS3231_STATUS_MAGIC_ADDR 0x0000
#define DS3231_LIGHTS_STATUS_ADDR 0x0001
#define DS3231_SWITCHES_STATUS_ADDR 0x0002

/* Alarm block: magic, onHour, onMinute, offHour, offMinute, enabled. */
#define DS3231_ALARM_MAGIC_ADDR 0x0008
#define DS3231_ALARM_ON_HOUR_ADDR 0x0009
#define DS3231_ALARM_ON_MINUTE_ADDR 0x000A
#define DS3231_ALARM_OFF_HOUR_ADDR 0x000B
#define DS3231_ALARM_OFF_MINUTE_ADDR 0x000C
#define DS3231_ALARM_ENABLED_ADDR 0x000D

typedef struct {
    uint8_t lights;
    uint8_t switches;
} DS3231_StatusSnapshot;

typedef struct {
    uint8_t onHour;
    uint8_t onMinute;
    uint8_t offHour;
    uint8_t offMinute;
    uint8_t enabled;
} DS3231_AlarmConfig;

/* Initialize the I2C bus used for the RTC and EEPROM. */
static inline void DS3231_EEPROM_Init(void)
{
    DS3231_I2C_Init();
}

/* Write len bytes at addr; must not cross a 32-byte page boundary. Returns 1 on success. */
static inline uint8_t DS3231_EEPROM_WriteBytes(uint16_t addr, const uint8_t *data, uint8_t len)
{
    uint8_t ok;

    DS3231_I2C_Start();
    ok = DS3231_I2C_WriteByte((uint8_t)(DS3231_EEPROM_I2C_ADDRESS << 1)) &&
         DS3231_I2C_WriteByte((uint8_t)(addr >> 8)) &&
         DS3231_I2C_WriteByte((uint8_t)(addr & 0xFFU));
    for (uint8_t i = 0U; ok && i < len; i++) {
        ok = DS3231_I2C_WriteByte(data[i]);
    }
    DS3231_I2C_Stop();

    if (!ok) {
        return 0U;
    }

    /* The EEPROM NACKs its address until the internal write cycle (up to ~10 ms) finishes. */
    for (uint8_t t = 0U; t < DS3231_EEPROM_WRITE_POLL_TRIES; t++) {
        DS3231_I2C_Start();
        ok = DS3231_I2C_WriteByte((uint8_t)(DS3231_EEPROM_I2C_ADDRESS << 1));
        DS3231_I2C_Stop();
        if (ok) {
            return 1U;
        }
        Delay_Ms(1);
    }

    return 0U;
}

/* Read len bytes starting at addr. Returns 1 on success. */
static inline uint8_t DS3231_EEPROM_ReadBytes(uint16_t addr, uint8_t *data, uint8_t len)
{
    DS3231_I2C_Start();
    if (!DS3231_I2C_WriteByte((uint8_t)(DS3231_EEPROM_I2C_ADDRESS << 1)) ||
        !DS3231_I2C_WriteByte((uint8_t)(addr >> 8)) ||
        !DS3231_I2C_WriteByte((uint8_t)(addr & 0xFFU))) {
        DS3231_I2C_Stop();
        return 0U;
    }

    DS3231_I2C_Start();
    if (!DS3231_I2C_WriteByte((uint8_t)((DS3231_EEPROM_I2C_ADDRESS << 1) | 1U))) {
        DS3231_I2C_Stop();
        return 0U;
    }

    for (uint8_t i = 0U; i < len; i++) {
        data[i] = DS3231_I2C_ReadByte((uint8_t)(i + 1U < len));
    }
    DS3231_I2C_Stop();

    return 1U;
}

/* Save the current light/switch status snapshot. */
static inline void DS3231_EEPROM_SaveStatus(uint8_t lightStatus, uint8_t switchStatus)
{
    uint8_t buf[3] = {DS3231_STATUS_MAGIC_VALUE, lightStatus, switchStatus};
    (void)DS3231_EEPROM_WriteBytes(DS3231_STATUS_MAGIC_ADDR, buf, sizeof(buf));
}

/* Read the saved light/switch snapshot. Outputs are 0 if nothing valid is stored. */
static inline void DS3231_EEPROM_LoadStatus(uint8_t *lightStatus, uint8_t *switchStatus)
{
    uint8_t buf[3] = {0U, 0U, 0U};

    if (!DS3231_EEPROM_ReadBytes(DS3231_STATUS_MAGIC_ADDR, buf, sizeof(buf)) ||
        buf[0] != DS3231_STATUS_MAGIC_VALUE) {
        buf[1] = 0U;
        buf[2] = 0U;
    }

    if (lightStatus != NULL) {
        *lightStatus = buf[1];
    }

    if (switchStatus != NULL) {
        *switchStatus = buf[2];
    }
}

static inline uint8_t DS3231_EEPROM_ReadLightStatus(void)
{
    uint8_t lights;
    DS3231_EEPROM_LoadStatus(&lights, NULL);
    return lights;
}

static inline uint8_t DS3231_EEPROM_ReadSwitchStatus(void)
{
    uint8_t switches;
    DS3231_EEPROM_LoadStatus(NULL, &switches);
    return switches;
}

static inline void DS3231_EEPROM_SaveSnapshot(const DS3231_StatusSnapshot *snapshot)
{
    if (snapshot != NULL) {
        DS3231_EEPROM_SaveStatus(snapshot->lights, snapshot->switches);
    }
}

static inline void DS3231_EEPROM_LoadSnapshot(DS3231_StatusSnapshot *snapshot)
{
    if (snapshot != NULL) {
        DS3231_EEPROM_LoadStatus(&snapshot->lights, &snapshot->switches);
    }
}

/* Store the daily alarm configuration. Returns 1 on success. */
static inline uint8_t DS3231_EEPROM_SaveAlarmConfig(const DS3231_AlarmConfig *cfg)
{
    if (cfg == NULL) {
        return 0U;
    }

    uint8_t buf[6] = {
        DS3231_ALARM_MAGIC_VALUE,
        cfg->onHour,
        cfg->onMinute,
        cfg->offHour,
        cfg->offMinute,
        cfg->enabled ? 1U : 0U,
    };

    return DS3231_EEPROM_WriteBytes(DS3231_ALARM_MAGIC_ADDR, buf, sizeof(buf));
}

/* Load the saved alarm configuration. Returns 1 if valid data was found; cfg is untouched otherwise. */
static inline uint8_t DS3231_EEPROM_LoadAlarmConfig(DS3231_AlarmConfig *cfg)
{
    uint8_t buf[6];

    if (cfg == NULL ||
        !DS3231_EEPROM_ReadBytes(DS3231_ALARM_MAGIC_ADDR, buf, sizeof(buf)) ||
        buf[0] != DS3231_ALARM_MAGIC_VALUE ||
        buf[1] > 23U || buf[2] > 59U || buf[3] > 23U || buf[4] > 59U) {
        return 0U;
    }

    cfg->onHour = buf[1];
    cfg->onMinute = buf[2];
    cfg->offHour = buf[3];
    cfg->offMinute = buf[4];
    cfg->enabled = buf[5] ? 1U : 0U;
    return 1U;
}

/* Default alarm values used at first boot or when no persistent setting exists. */
static inline DS3231_AlarmConfig DS3231_EEPROM_GetDefaultAlarmConfig(void)
{
    DS3231_AlarmConfig cfg;
    cfg.onHour = 18U;
    cfg.onMinute = 0U;
    cfg.offHour = 6U;
    cfg.offMinute = 0U;
    cfg.enabled = 1U;
    return cfg;
}

#endif
