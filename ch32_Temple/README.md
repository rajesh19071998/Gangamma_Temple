# CH32 Temple Control Board

This firmware controls a CH32V003-based lighting panel with:

<!--
Project overview:
- Four controlled lighting outputs
- Four input switches with pull-up resistors
- DS3231 RTC over I2C
- EEPROM-backed state and alarm storage
- UART and command interface for remote control
-->

- 4 output lights
- 4 input switches
- DS3231 RTC module (bit-banged I2C)
- AT24C32 EEPROM on the RTC module for state and alarm storage
- UART command interface (interrupt-driven receive)
- Daily alarm schedule with enable/disable option

## 1. GPIO Connections

### Lights (Outputs)
These are the GPIO outputs used to drive the lamps.

- Light 1 -> PC3
- Light 2 -> PC4
- Light 3 -> PC6
- Light 4 -> PC7

### Switches (Inputs with pull-up)
These are input pins for the physical switches. They are configured as pull-up inputs.

- Switch 1 -> PD4
- Switch 2 -> PC0
- Switch 3 -> PA1
- Switch 4 -> PA2

### DS3231 RTC Module (I2C)
The RTC and its on-board EEPROM share one software (bit-banged) I2C bus. External pull-ups are required; most DS3231 modules include them.

- SDA -> PC1
- SCL -> PC2
- DS3231 RTC address: 0x68
- AT24C32 EEPROM address: 0x57

### UART
UART communication is used for remote commands and status reporting (USART1, 115200 8N1).

- TX -> PD5
- RX -> PD6

## 2. Functionality

### Light Control
Light control is event driven. A light changes state only when something changes, and the most recent event wins:

- A physical switch changing state sets its light to the switch state (ON/OFF).
- A UART `LxON` / `LxOFF` command sets that light.
- The daily alarm sets all lights ON or OFF at the configured times.

There is no persistent UART override: a later switch change takes control of that light again.

### Switch Behavior
- Each switch is read as an active-low input with internal pull-up.
- Switch changes are debounced with a 100 ms delay.

### Startup
- The firmware waits 10 seconds after reset before initializing peripherals.
- The alarm configuration is loaded from EEPROM. If nothing valid is stored, the defaults (ON 18:00, OFF 06:00, enabled) are used.

### RTC and Time
- The DS3231 RTC provides real-time date and time.
- The device sends the current RTC timestamp every time the minute changes.
- Example output:

```text
RTC:2026-10-13 18:05:00
```

### EEPROM Storage
The DS3231 chip itself has no user memory, so data is stored in the AT24C32 EEPROM (address 0x57) found on DS3231 modules.

| Address | Content |
|---------|---------|
| 0x0000 | status magic (0xA5) |
| 0x0001 | light bitmask (bit 0 = light 1) |
| 0x0002 | switch bitmask (bit 0 = switch 1) |
| 0x0008 | alarm magic (0x5A) |
| 0x0009-0x000C | alarm ON hour/minute, OFF hour/minute |
| 0x000D | alarm enable flag |

- The light and switch bitmasks are written only when they change.
- The alarm block is written when the alarm times or enable flag change.
- Stored data is ignored unless its magic byte and value ranges are valid.
- Each write waits for the EEPROM internal write cycle (up to about 10 ms).

## 3. UART Commands

All commands are sent as plain text over UART at 115200 baud and must end with CR, LF or `;`. Maximum command length is 31 characters.

### Light commands

#### Turn light ON
```text
L1ON
L2ON
L3ON
L4ON
```

#### Turn light OFF
```text
L1OFF
L2OFF
L3OFF
L4OFF
```

#### Status response
If a light command is accepted, the controller sends a status line such as:

```text
L1:ON
```

### RTC commands

#### Set RTC time and date
```text
SETTIME=2026-10-13 18:05:00
```

This will update the DS3231 clock immediately.

#### Read RTC time
The system sends the current date/time automatically every minute, for example:

```text
RTC:2026-10-13 18:05:00
```

### Status commands

#### Read all light and switch status
```text
ALLSTATUS
```

Example response:

```text
STATUS:L1=ON L2=OFF L3=OFF L4=ON S1=ON S2=OFF S3=ON S4=OFF
```

### Alarm commands

#### Set alarm times
```text
SETALARM=18:00,06:00
```
This means:
- turn all lights ON at 18:00
- turn all lights OFF at 06:00

#### Get current alarm settings
```text
GETALARM
```

Example response:

```text
ALARM:ENABLED=ON ON=18:00 OFF=06:00
```

#### Enable alarm
```text
ALARMON
```
or
```text
ALARMENABLE
```

#### Disable alarm
```text
ALARMOFF
```
or
```text
ALARMDISABLE
```

Example response after toggle:

```text
ALARM:ENABLED=OFF ON=18:00 OFF=06:00
```

## 4. Alarm Operation

The alarm feature runs once per minute and compares the current RTC time with the configured ON/OFF values.

### Example alarm schedule
```text
ON  = 18:00
OFF = 06:00
```

At 18:00 the system turns all lights ON.
At 06:00 the system turns all lights OFF.

### Alarm enable/disable behavior
- If the alarm is enabled, the configured schedule is active.
- If the alarm is disabled, the automatic daily schedule is skipped.
- The alarm enabled state is stored in EEPROM, so it remains after power loss.

## 5. Example Workflow

### Enable alarm and set schedule
```text
SETALARM=18:00,06:00
ALARMON
GETALARM
```

### Disable alarm
```text
ALARMOFF
GETALARM
```

### Read full system status
```text
ALLSTATUS
```

### Set RTC clock
```text
SETTIME=2026-10-13 18:05:00
```

### Force a specific light on
```text
L1ON
```

### Return to switch-based control
Move the physical switch; the light follows the new switch state.

## 6. Build and Run

Compile the firmware from the project root:

```powershell
platformio run
```

Flash to the board with the PlatformIO upload target (`wlink`):

```powershell
platformio run --target upload --upload-port COM33
```

Open the serial monitor on the UART TX/RX pins at 115200 baud:

```powershell
platformio device monitor --port COM33
```

## 7. Notes

- The alarm flag is stored in EEPROM, not just RAM.
- UART bytes are received by an interrupt into a ring buffer, then assembled into commands and queued.
- The scheduler checks time on each minute transition and applies the configured ON/OFF to all lights.
- If the RTC does not respond, the time reads as 00:00 and the alarm may trigger unexpectedly at 00:00 if it is set to that time.
- Actual hardware validation should be done on the physical board with the correct GPIO wiring and serial terminal connected.
