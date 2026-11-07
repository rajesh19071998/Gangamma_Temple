#pragma once

#include <Arduino.h>

constexpr uint8_t LIGHT_COUNT = 4;

// ESP-12F pins safe at boot: GPIO4 (D2), GPIO5 (D1), GPIO12 (D6), GPIO13 (D7).
constexpr uint8_t LIGHT_PINS[LIGHT_COUNT] = {5, 4, 12, 13};

// Set to LOW for active-low relay boards.
constexpr uint8_t LIGHT_ON_LEVEL = HIGH;

static bool lightState[LIGHT_COUNT] = {false, false, false, false};

inline void Lights_Set(uint8_t index, bool on)
{
    if (index >= LIGHT_COUNT) {
        return;
    }
    lightState[index] = on;
    digitalWrite(LIGHT_PINS[index], on ? LIGHT_ON_LEVEL : !LIGHT_ON_LEVEL);
}

inline bool Lights_Get(uint8_t index)
{
    return index < LIGHT_COUNT && lightState[index];
}

inline void Lights_Init()
{
    for (uint8_t i = 0; i < LIGHT_COUNT; i++) {
        pinMode(LIGHT_PINS[i], OUTPUT);
        Lights_Set(i, false);
    }
}
