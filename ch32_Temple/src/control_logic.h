#ifndef CONTROL_LOGIC_H
#define CONTROL_LOGIC_H

#include <stdbool.h>
#include <stdint.h>

#include "lights.h"
#include "switches.h"

static inline void UART_Comm_ApplyCommand(const char *cmd); // defined in uart_comm.h

bool uart_lights[4] = {false, false, false, false};
bool uart_lights_old[4] = {false, false, false, false};
bool actual_lights[4] = {false, false, false, false};
bool actual_lights_old[4] = {false, false, false, false};

void applylights_old_state(void)
{
    for (int i = 0; i < 4; i++) {
        if(actual_lights_old[i] != actual_lights[i])
        {
            actual_lights[i] = uart_lights_old[i] = uart_lights[i] = actual_lights_old[i]; // update all flags with stored status
            Light_control_Set(i, actual_lights[i]);
            UART_Comm_ApplyCommand("ALLSTATUS");
        }
    }
}

void UpdateActualLights(void)
{
    for (int i = 0; i < 4; i++) {
        if(uart_lights_old[i] != uart_lights[i]) 
        {
            actual_lights[i] = uart_lights[i];
            Light_control_Set(i, actual_lights[i]);
            uart_lights_old[i] = uart_lights[i];
        }
        else if(switch_status_old[i] != switch_status[i])
        {
            actual_lights[i] = switch_status[i];
            Light_control_Set(i, actual_lights[i]);
            Delay_Ms(100);// switch debounce
            switch_status_old[i] = switch_status[i];
            UART_Comm_ApplyCommand("ALLSTATUS");
        }
    }
}

void LightONAlarm(void)
{
    for (int i = 0; i < 4; i++) {
        actual_lights[i] = true;
        Light_control_Set(i, actual_lights[i]);
        UART_Comm_ApplyCommand("ALLSTATUS");
    }
}

void LightOFFAlarm(void)
{
    for (int i = 0; i < 4; i++) {
        actual_lights[i] = false;
        Light_control_Set(i, actual_lights[i]);
        UART_Comm_ApplyCommand("ALLSTATUS");
    }
}

#endif /* CONTROL_LOGIC_H */