#ifndef UART_COMM_H // Prevent multiple inclusion of the UART communication header.
#define UART_COMM_H // Mark the UART communication header as included.

#include <stdio.h> // Include standard C I/O functions like snprintf and sscanf.
#include <string.h> // Include string helpers such as strcmp, strncmp, and strncpy.
#include <ch32v00x.h> // Include the CH32 peripheral register definitions and GPIO/USART APIs.

#include "ds3231.h" // Include the DS3231 RTC access helper functions used for time reads and writes.
//#include "lights.h" // Include the light output abstraction used by UART light commands.
//#include "switches.h" // Include the switch input abstraction used by status reporting.
#include "control_logic.h" // Include the control logic layer for UART light state management.
/*
 * UART communication layer.
 * This module receives commands, queues them, applies the command logic, and returns status
 * over the serial port. It also handles light overrides and periodic RTC timestamp output.
 */ // End of the UART module overview comment.

static inline void Alarms_SetAlarmTimes(uint8_t onHour, uint8_t onMinute, uint8_t offHour, uint8_t offMinute); // Declare alarm time setter before its definition is available.
static inline void Alarms_SetEnabled(uint8_t enabled); // Declare alarm enable/disable function before its definition is available.
static inline void Alarms_SendListOverUart(void); // Declare alarm list sender before its definition is available.

#define UART_COMM_BAUD_RATE 115200U // Set the UART baud rate to 115200 bits per second.
#define UART_COMM_LINE_LEN 32U // Set the maximum command line length.
#define UART_COMM_QUEUE_LEN 8U // Set the maximum number of queued UART commands.

#define UART_COMM_RX_RING_LEN 64U // Set the size of the interrupt-fed raw receive ring buffer.

/* Raw RX ring buffer filled by the USART1 interrupt and drained by UART_Comm_ProcessRx(). */ // Describe the ISR-to-main handoff buffer.
static volatile uint8_t uart_rx_ring[UART_COMM_RX_RING_LEN]; // Store received bytes until the main loop consumes them.
static volatile uint8_t uart_rx_head = 0; // Index written by the interrupt handler.
static volatile uint8_t uart_rx_tail = 0; // Index read by the main loop.

/* USART1 receive interrupt: move each received byte into the ring buffer so none are overrun. */ // Describe the ISR.
void USART1_IRQHandler(void) __attribute__((interrupt("WCH-Interrupt-fast"))); // Declare the handler as a fast interrupt routine.
void USART1_IRQHandler(void) // Override the weak default USART1 vector.
{
    if (USART1->STATR & USART_FLAG_RXNE) { // Check that a received byte is waiting.
        uint8_t data = (uint8_t)USART1->DATAR; // Read the byte, which also clears RXNE and any overrun flag.
        uint8_t next = (uint8_t)((uart_rx_head + 1U) % UART_COMM_RX_RING_LEN); // Compute the next head position.
        if (next != uart_rx_tail) { // Drop the byte only if the ring buffer is full.
            uart_rx_ring[uart_rx_head] = data; // Store the received byte.
            uart_rx_head = next; // Publish the new head index.
        }
    }
}

/* UART receive buffer and command queue */ // Introduce the UART receive and queue state.
static char uart_line_buffer[UART_COMM_LINE_LEN]; // Store the current incoming UART command being assembled.
static uint8_t uart_line_index = 0; // Track the current character index inside the UART command buffer.

static char uart_cmd_queue[UART_COMM_QUEUE_LEN][UART_COMM_LINE_LEN]; // Create a circular queue for pending UART commands.
static uint8_t uart_cmd_head = 0; // Track the head index of the command queue.
static uint8_t uart_cmd_tail = 0; // Track the tail index of the command queue.
static uint8_t uart_cmd_count = 0; // Track how many commands are currently queued.

/* Manual pipe-like override values for lights, with -1 meaning "no override". */ // Explain the override array semantics.
//static int8_t uart_light_override[4] = {-1, -1, -1, -1}; //rjv Default all four light outputs to automatic switch-driven control.

/* Add a command string to the queue for later processing. */ // Describe the purpose of the queue insertion helper.
static inline void UART_Comm_QueueCommand(const char *cmd) // Queue a command string for later processing.
{
    if (cmd == NULL || uart_cmd_count >= UART_COMM_QUEUE_LEN) { // Ignore null input or refuse to queue when full.
        return; // Exit without changing the queue state.
    }

    strncpy(uart_cmd_queue[uart_cmd_tail], cmd, UART_COMM_LINE_LEN - 1); // Copy the command into the circular queue slot.
    uart_cmd_queue[uart_cmd_tail][UART_COMM_LINE_LEN - 1] = '\0'; // Ensure the stored string is always null-terminated.
    uart_cmd_tail = (uart_cmd_tail + 1U) % UART_COMM_QUEUE_LEN; // Advance the tail index in the circular queue.
    uart_cmd_count++; // Increase the queued-command count.
}

static inline void UART_Comm_SendString(const char *text); // Forward declare the string-send helper before its implementation.
static inline void UART_Comm_SendRtcTimestamp(void); // Forward declare the RTC timestamp sender before its implementation.

/* Handle one UART command and mutate the controller state as required. */ // Introduce the command interpreter block.
static inline void UART_Comm_ApplyCommand(const char *cmd) // Execute one parsed UART command.
{
    if (cmd == NULL) { // Ignore null commands to prevent crashes.
        return; // Exit immediately for invalid input.
    }

    if (strncmp(cmd, "SETTIME=", 8) == 0) { // Check whether the command is a set-time instruction.
        DS3231_DateTime dt = {0}; // Create a date/time structure with zeros before loading parsed values.
        int year = 0, month = 0, date = 0, hour = 0, minute = 0, second = 0; // Prepare conversion variables for the time parser.
        if (sscanf(cmd + 8, "%4d-%2d-%2d %2d:%2d:%2d",
                   &year, &month, &date, &hour, &minute, &second) == 6) { // Parse the expected RTC date and time format.
            dt.year = (uint8_t)(year % 100U); // Store the year in the DS3231 two-digit format.
            dt.month = (uint8_t)month; // Store the month from the parsed input.
            dt.date = (uint8_t)date; // Store the date from the parsed input.
            dt.hours = (uint8_t)hour; // Store the hour from the parsed input.
            dt.minutes = (uint8_t)minute; // Store the minute from the parsed input.
            dt.seconds = (uint8_t)second; // Store the second from the parsed input.
            dt.day = 1; // Set the day-of-week placeholder because the command format does not include it.
            DS3231_WriteDateTime(&dt); // Write the new RTC time to the DS3231.
            UART_Comm_SendRtcTimestamp(); // Immediately echo the new time back to the serial console.
        }
        return; // Leave the command interpreter after handling the SETTIME command.
    }

    if (strncmp(cmd, "SETALARM=", 9) == 0) { // Check whether the command is a set-alarm instruction.
        int onHour = 0, onMinute = 0, offHour = 0, offMinute = 0; // Prepare variables for the ON/OFF alarm times.
        if (sscanf(cmd + 9, "%2d:%2d,%2d:%2d",
                   &onHour, &onMinute, &offHour, &offMinute) == 4) { // Parse the alarm times from the command.
            Alarms_SetAlarmTimes((uint8_t)onHour, (uint8_t)onMinute, (uint8_t)offHour, (uint8_t)offMinute); // Save the new alarm schedule to EEPROM.
            Alarms_SendListOverUart(); // Send the updated alarm settings over UART.
        }
        return; // Leave the command interpreter after handling the SETALARM command.
    }

    if (strcmp(cmd, "ALARMON") == 0 || strcmp(cmd, "ALARMENABLE") == 0) { // Check for the alarm enable command.
        Alarms_SetEnabled(1); // Enable the daily alarm scheduler.
        Alarms_SendListOverUart(); // Echo the updated state to the host.
        return; // Leave the command interpreter.
    }

    if (strcmp(cmd, "ALARMOFF") == 0 || strcmp(cmd, "ALARMDISABLE") == 0) { // Check for the alarm disable command.
        Alarms_SetEnabled(0); // Disable the daily alarm scheduler.
        Alarms_SendListOverUart(); // Echo the updated state to the host.
        return; // Leave the command interpreter.
    }

    if (strcmp(cmd, "GETALARM") == 0) { // Check for the get-alarm command.
        Alarms_SendListOverUart(); // Send the alarm list and current enabled state over UART.
        return; // Leave the command interpreter.
    }

    if (strcmp(cmd, "ALLSTATUS") == 0) { // Check for the all-status command.
        char msg[96]; // Allocate a buffer for the status summary string.
        uint8_t light1 = actual_lights[0]; // Resolve the current effective state of light 1.
        uint8_t light2 = actual_lights[1]; // Resolve the current effective state of light 2.
        uint8_t light3 = actual_lights[2]; // Resolve the current effective state of light 3.
        uint8_t light4 = actual_lights[3]; // Resolve the current effective state of light 4.
        uint8_t sw1 = Switch_IsOn(SWITCH_1); // Read physical switch 1 state.
        uint8_t sw2 = Switch_IsOn(SWITCH_2); // Read physical switch 2 state.
        uint8_t sw3 = Switch_IsOn(SWITCH_3); // Read physical switch 3 state.
        uint8_t sw4 = Switch_IsOn(SWITCH_4); // Read physical switch 4 state.

        snprintf(msg, sizeof(msg), "STATUS:L1=%s L2=%s L3=%s L4=%s S1=%s S2=%s S3=%s S4=%s\r\n",
                 light1 ? "ON" : "OFF",
                 light2 ? "ON" : "OFF",
                 light3 ? "ON" : "OFF",
                 light4 ? "ON" : "OFF",
                 sw1 ? "ON" : "OFF",
                 sw2 ? "ON" : "OFF",
                 sw3 ? "ON" : "OFF",
                 sw4 ? "ON" : "OFF"); // Build a formatted summary of all light and switch states.
        UART_Comm_SendString(msg); // Send the full summary string over UART.
        return; // Leave the command interpreter.
    }

    if (strcmp(cmd, "L1ON") == 0) { // Check whether the command requests light 1 ON.
        uart_lights[0] = true; // Update the control logic layer to reflect the override state.
    } else if (strcmp(cmd, "L1OFF") == 0) { // Check whether the command requests light 1 OFF.
        uart_lights[0] = false; // Update the control logic layer to reflect the override state.
    }
     else if (strcmp(cmd, "L2ON") == 0) { // Check whether the command requests light 2 ON.
        uart_lights[1] = true; // Update the control logic layer to reflect the override state.
    } else if (strcmp(cmd, "L2OFF") == 0) { // Check whether the command requests light 2 OFF.
        uart_lights[1] = false; // Update the control logic layer to reflect the override state.
    
    } else if (strcmp(cmd, "L3ON") == 0) { // Check whether the command requests light 3 ON.
        uart_lights[2] = true; // Update the control logic layer to reflect the override state.
    } else if (strcmp(cmd, "L3OFF") == 0) { // Check whether the command requests light 3 OFF.
        uart_lights[2] = false; // Update the control logic layer to reflect the override state. 
    } else if (strcmp(cmd, "L4ON") == 0) { // Check whether the command requests light 4 ON.
        uart_lights[3] = true; // Update the control logic layer to reflect the override state.
    } else if (strcmp(cmd, "L4OFF") == 0) { // Check whether the command requests light 4 OFF.
        uart_lights[3] = false; // Update the control logic layer to reflect the override state.
    } 
    
}

/* Send the state of one specific light after a UART command. */ // Describe the single-light response helper.
static inline void UART_Comm_SendLightStatus(uint8_t lightIndex) // Send the current state of one light over UART.
{
    char msg[32]; // Allocate a buffer for the single-light status string.
    uint8_t state = 0; // Hold the current logical state for the target light.
  
    if (lightIndex >= 1 && lightIndex <= 4) { // Ignore invalid light indices. 
        UpdateActualLights();
        state = actual_lights[lightIndex - 1]; //rjv need to check Save the effective light state in the local variable.
        snprintf(msg, sizeof(msg), "L%d:%s\r\n", lightIndex, state ? "ON" : "OFF"); // Format the light status string.
        UART_Comm_SendString(msg); // Send the single-light status packet over UART.
    }
}

/* Drain all queued commands and process them in order. */ // Describe the command draining loop.
static inline void UART_Comm_ApplyPendingCommands(void) // Process all queued commands in FIFO order.
{
    while (uart_cmd_count > 0U) { // Continue until the queue is empty.
        char *cmd = uart_cmd_queue[uart_cmd_head]; // Read the next queued command string.
        UART_Comm_ApplyCommand(cmd); // Execute the command logic.

        if (strcmp(cmd, "L1ON") == 0 || strcmp(cmd, "L1OFF") == 0 ) { // Detect light 1 change commands.
            UART_Comm_SendLightStatus(1); // Report the new light 1 state.
        } else if (strcmp(cmd, "L2ON") == 0 || strcmp(cmd, "L2OFF") == 0 ) { // Detect light 2 change commands.
            UART_Comm_SendLightStatus(2); // Report the new light 2 state.
        } else if (strcmp(cmd, "L3ON") == 0 || strcmp(cmd, "L3OFF") == 0 ) { // Detect light 3 change commands.
            UART_Comm_SendLightStatus(3); // Report the new light 3 state.
        } else if (strcmp(cmd, "L4ON") == 0 || strcmp(cmd, "L4OFF") == 0 ) { // Detect light 4 change commands.
            UART_Comm_SendLightStatus(4); // Report the new light 4 state.
        }

        uart_cmd_head = (uart_cmd_head + 1U) % UART_COMM_QUEUE_LEN; // Advance the queue head to the next entry.
        uart_cmd_count--; // Reduce the queued command count.
    }
}

/* Receive UART bytes and build complete command strings terminated by CR, LF or semicolon. */ // Explain the serial receive path.
static inline void UART_Comm_ProcessRx(void) // Read all available UART bytes and queue complete commands.
{
    while (uart_rx_tail != uart_rx_head) { // Keep reading while bytes are waiting in the ring buffer.
        char c = (char)uart_rx_ring[uart_rx_tail]; // Read the next byte captured by the interrupt handler.
        uart_rx_tail = (uint8_t)((uart_rx_tail + 1U) % UART_COMM_RX_RING_LEN); // Advance the tail index.

        if (c == '\r' || c == '\n' || c == ';') { // Detect command terminators.
            if (uart_line_index > 0U) { // Only queue non-empty commands.
                uart_line_buffer[uart_line_index] = '\0'; // Null-terminate the assembled command string.
                UART_Comm_QueueCommand(uart_line_buffer); // Push the complete command into the queue.
                uart_line_index = 0U; // Reset the input buffer for the next command.
            }
        } else if (uart_line_index < (UART_COMM_LINE_LEN - 1U)) { // Keep adding characters while the buffer has room.
            uart_line_buffer[uart_line_index++] = c; // Store the received byte and advance the buffer index.
        } else { // Overflow protection for too-long command lines.
            uart_line_index = 0U; // Reset the buffer on overflow.
        }
    }
}

/* Configure USART1 for UART serial communication with the default CH32 remapped pins. */ // Describe the serial-port initialization logic.
static inline void UART_Comm_Init(void) // Initialize the UART peripheral and GPIO pins.
{
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_USART1 | RCC_APB2Periph_GPIOD, ENABLE); // Enable the USART1 and GPIOD clocks.

    GPIO_PinRemapConfig(GPIO_PartialRemap1_USART1, ENABLE); // Remap USART1 TX/RX to the alternate CH32 pins.

    GPIO_InitTypeDef gpio = {0}; // Start with a zero-initialized GPIO configuration structure.
    gpio.GPIO_Pin = GPIO_Pin_5; // Set TX pin to PD5.
    gpio.GPIO_Mode = GPIO_Mode_AF_PP; // Configure TX as alternate-function push-pull.
    gpio.GPIO_Speed = GPIO_Speed_50MHz; // Set the output speed to 50 MHz.
    GPIO_Init(GPIOD, &gpio); // Initialize the TX pin on GPIOD.

    gpio.GPIO_Pin = GPIO_Pin_6; // Set RX pin to PD6.
    gpio.GPIO_Mode = GPIO_Mode_IN_FLOATING; // Configure RX as a floating input.
    GPIO_Init(GPIOD, &gpio); // Initialize the RX pin on GPIOD.

    USART_InitTypeDef uart = {0}; // Start with a zero-initialized USART configuration structure.
    uart.USART_BaudRate = UART_COMM_BAUD_RATE; // Set the serial baud rate.
    uart.USART_WordLength = USART_WordLength_8b; // Use 8-bit data words.
    uart.USART_StopBits = USART_StopBits_1; // Use one stop bit.
    uart.USART_Parity = USART_Parity_No; // Disable parity.
    uart.USART_Mode = USART_Mode_Rx | USART_Mode_Tx; // Enable both RX and TX mode.
    uart.USART_HardwareFlowControl = USART_HardwareFlowControl_None; // Disable hardware flow control.
    USART_Init(USART1, &uart); // Apply the configured UART settings to USART1.
    USART_ITConfig(USART1, USART_IT_RXNE, ENABLE); // Enable the receive-not-empty interrupt.
    NVIC_EnableIRQ(USART1_IRQn); // Enable the USART1 interrupt in the interrupt controller.
    USART_Cmd(USART1, ENABLE); // Enable USART1 operation.
}

#if 0
/* Resolve the current light output state using a UART override when present, otherwise the switch state. */ // Describe the output-state selection logic.
static inline uint8_t UART_Comm_GetLightState(LightIndex light, uint8_t switchState) // Return the effective state for a light channel.
{
    if (light >= LIGHT_1 && light <= LIGHT_4) { // Validate the logical light index.
        if (uart_light_override[light] >= 0) { // Check whether a manual override is active.
            return (uint8_t)uart_light_override[light]; // Return the override state instead of the switch state.
        }
    }

    return switchState; // Otherwise return the current switch-derived state.
}
#endif

/* Read the RTC and send the timestamp message over UART once per minute. */ // Describe the periodic timestamp helper.
static inline void UART_Comm_SendRtcTimestamp(void) // Publish the current RTC date and time over UART.
{
    DS3231_DateTime dt = DS3231_ReadDateTime(); // Read the current RTC value.
    char msg[64]; // Allocate a buffer for the timestamp message.

    snprintf(msg, sizeof(msg), "RTC:%04u-%02u-%02u %02u:%02u:%02u\r\n",
             (unsigned int)(2000U + dt.year),
             (unsigned int)dt.month,
             (unsigned int)dt.date,
             (unsigned int)dt.hours,
             (unsigned int)dt.minutes,
             (unsigned int)dt.seconds); // Format the RTC timestamp string.

    UART_Comm_SendString(msg); // Send the timestamp message over UART.
}

/* Send a zero-terminated string over the UART TX line. */ // Describe the UART transmit helper.
static inline void UART_Comm_SendString(const char *text) // Transmit a null-terminated string via UART.
{
    if (text == NULL) { // Ignore null pointers to avoid invalid dereference.
        return; // Exit without sending anything.
    }

    while (*text != '\0') { // Continue until the terminating null byte is reached.
        while (USART_GetFlagStatus(USART1, USART_FLAG_TXE) == RESET) {} // Wait until the TX buffer is ready for another byte.
        USART_SendData(USART1, (uint16_t)(*text++)); // Send one character and advance to the next byte.
    }
}

#endif // End of the UART header guard.

