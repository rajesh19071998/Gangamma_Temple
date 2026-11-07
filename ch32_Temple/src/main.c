#include <ch32v00x.h>

/*
 * Main application entry for the CH32 Temple lighting controller.
 * This loop initializes all subsystems, watches the RTC, processes UART commands,
 * and updates the output lights according to switch state, UART override state,
 * and the configured daily alarm schedule.
 */
#include "ds3231_eeprom.h"
//#include "lights.h"
//#include "switches.h"
#include "control_logic.h"
#include "uart_comm.h"
#include "alarms.h"

/* Fault handlers are kept intentionally minimal to stop the controller safely */
void NMI_Handler(void) __attribute__((interrupt("WCH-Interrupt-fast")));
void HardFault_Handler(void) __attribute__((interrupt("WCH-Interrupt-fast")));

int main(void) {
    /* Track the last RTC minute so we only send timestamp updates once per minute */
    uint8_t lastRtcMinute = 0xFF;
    Delay_Init();
    Delay_Ms(10000);
    /* Restore EEPROM-backed alarm configuration before normal operation starts */
    DS3231_EEPROM_Init();
    Alarms_LoadFromEeprom();
    Lights_Init();
    Switches_Init();
    UART_Comm_Init();

    /* Seed with the stored snapshot so an unchanged state is not rewritten at boot */
    uint8_t savedLights;
    uint8_t savedSwitches;
    DS3231_EEPROM_LoadStatus(&savedLights, &savedSwitches);
    for (int i = 0; i < 4; i++) {
        actual_lights_old[i] = (savedLights & (1U << i)) ? true : false;
        switch_status_old[i] = (savedSwitches & (1U << i)) ? true : false;
    }
    applylights_old_state();

    /* Main supervisory loop */
    while (1) {
        /* Read and queue incoming UART commands */
        UART_Comm_ProcessRx();
        UART_Comm_ApplyPendingCommands();

        /* Read current RTC time and trigger minute-based actions */
        DS3231_DateTime dt = DS3231_ReadDateTime();
        if (lastRtcMinute != dt.minutes) {
            if (lastRtcMinute != 0xFF) {
                UART_Comm_SendRtcTimestamp();
            }
            lastRtcMinute = dt.minutes;
            Alarms_ProcessDailySchedule();
        }

        /* Read the physical switch inputs */
        Get_Switch_status();
        UpdateActualLights();

        /* Persist the light/switch bitmasks (bit n = channel n+1) only when they change */
        uint8_t lights =   (uint8_t)((actual_lights[0] ? 1U : 0U) | (actual_lights[1] ? 2U : 0U) | (actual_lights[2] ? 4U : 0U) | (actual_lights[3] ? 8U : 0U));
        uint8_t switches = (uint8_t)((switch_status[0] ? 1U : 0U) | (switch_status[1] ? 2U : 0U) | (switch_status[2] ? 4U : 0U) | (switch_status[3] ? 8U : 0U));
        if (lights != savedLights || switches != savedSwitches) {
            DS3231_EEPROM_SaveStatus(lights, switches);
            savedLights = lights;
            savedSwitches = switches;
        }
    }

    return 0;
}

void NMI_Handler(void) {}
void HardFault_Handler(void)
{
    while (1)
    {
        /* Stay here on unrecoverable fault conditions. */
    }
}