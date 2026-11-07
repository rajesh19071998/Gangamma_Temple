#pragma once

#include <ESP8266WebServer.h>
#include <ESP8266HTTPUpdateServer.h>

static ESP8266HTTPUpdateServer httpUpdater;

// Adds a password-protected firmware upload page at /update.
inline void Ota_Init(ESP8266WebServer &server, const char *user, const char *password)
{
    httpUpdater.setup(&server, "/update", user, password);
}
