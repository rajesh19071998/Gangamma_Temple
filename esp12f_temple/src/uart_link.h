#pragma once

#include <Arduino.h>

constexpr uint32_t UART_REPLY_TIMEOUT_MS = 1500;

// Non-blocking: returns true when a complete line from the CH32 has been read into `line`.
inline bool UartLink_ReadLine(String &line)
{
    static String pending;
    while (Serial.available()) {
        char c = (char)Serial.read();
        if (c == '\n') {
            pending.trim();
            line = pending;
            pending = "";
            if (line.length() > 0) {
                return true;
            }
        } else if (pending.length() < 120) {
            pending += c;
        }
    }
    return false;
}

// Send one command line to the CH32 and wait for a reply line starting with `prefix`.
inline bool UartLink_Request(const String &cmd, const char *prefix, String &reply)
{
    while (Serial.available()) {
        Serial.read();  // drop stale replies such as "L1:ON"
    }

    Serial.print(cmd);
    Serial.print('\n');

    String line;
    uint32_t start = millis();
    while (millis() - start < UART_REPLY_TIMEOUT_MS) {
        while (Serial.available()) {
            char c = (char)Serial.read();
            if (c == '\n') {
                line.trim();
                if (line.startsWith(prefix)) {
                    reply = line;
                    return true;
                }
                line = "";
            } else if (line.length() < 120) {
                line += c;
            }
        }
        delay(1);
    }

    return false;
}
