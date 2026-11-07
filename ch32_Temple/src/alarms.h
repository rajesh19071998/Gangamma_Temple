#ifndef ALARMS_H
#define ALARMS_H

#include <ch32v00x.h>

#include "ds3231.h"
#include "ds3231_eeprom.h"
#include "lights.h"
#include "uart_comm.h"

/*
 * Daily alarm scheduler.
 * Alarm times are stored in the RTC-backed EEPROM and are applied once per minute.
 */
static DS3231_AlarmConfig g_alarmConfig = {18U, 0U, 6U, 0U, 1U};

static inline void Alarms_SetAlarmTimes(uint8_t onHour, uint8_t onMinute, uint8_t offHour, uint8_t offMinute);
static inline void Alarms_SendListOverUart(void);

/* Compare a given RTC hour/minute with the configured alarm value. */
static inline uint8_t Alarms_IsTimeMatch(uint8_t hour, uint8_t minute, uint8_t targetHour, uint8_t targetMinute)
{
    return (hour == targetHour && minute == targetMinute) ? 1U : 0U;
}

/* Load the alarm state from the RTC SRAM-backed EEPROM storage. */
static inline void Alarms_LoadFromEeprom(void)
{
    DS3231_AlarmConfig cfg = DS3231_EEPROM_GetDefaultAlarmConfig();
    DS3231_EEPROM_LoadAlarmConfig(&cfg);
    g_alarmConfig = cfg;
}

/* Persist the current alarm configuration into the RTC EEPROM storage. */
static inline void Alarms_SaveToEeprom(void)
{
    DS3231_EEPROM_SaveAlarmConfig(&g_alarmConfig);
}

/* Enable or disable the daily alarm schedule and preserve the flag in EEPROM. */
static inline void Alarms_SetEnabled(uint8_t enabled)
{
    g_alarmConfig.enabled = enabled ? 1U : 0U;
    Alarms_SaveToEeprom();
}

/* Return the current enabled state of the alarm schedule. */
static inline uint8_t Alarms_IsEnabled(void)
{
    return g_alarmConfig.enabled ? 1U : 0U;
}

/* Update the configured ON and OFF times for the alarm schedule. */
static inline void Alarms_SetAlarmTimes(uint8_t onHour, uint8_t onMinute, uint8_t offHour, uint8_t offMinute)
{
    g_alarmConfig.onHour = onHour;
    g_alarmConfig.onMinute = onMinute;
    g_alarmConfig.offHour = offHour;
    g_alarmConfig.offMinute = offMinute;
    Alarms_SaveToEeprom();
}

/* Send the current alarm configuration as a UART text message. */
static inline void Alarms_SendListOverUart(void)
{
    char msg[96];
    snprintf(msg, sizeof(msg), "ALARM:ENABLED=%s ON=%02u:%02u OFF=%02u:%02u\r\n",
             g_alarmConfig.enabled ? "ON" : "OFF",
             g_alarmConfig.onHour, g_alarmConfig.onMinute,
             g_alarmConfig.offHour, g_alarmConfig.offMinute);
    UART_Comm_SendString(msg);
}

/* Execute the daily alarm check and apply ON/OFF overrides when the RTC time matches. */
static inline void Alarms_ProcessDailySchedule(void)
{
    if (!Alarms_IsEnabled()) {
        return;
    }

    DS3231_DateTime dt = DS3231_ReadDateTime();

    if (Alarms_IsTimeMatch(dt.hours, dt.minutes, g_alarmConfig.onHour, g_alarmConfig.onMinute)) {
        LightONAlarm();
    } else if (Alarms_IsTimeMatch(dt.hours, dt.minutes, g_alarmConfig.offHour, g_alarmConfig.offMinute)) {
        LightOFFAlarm();
    }
}

#endif
