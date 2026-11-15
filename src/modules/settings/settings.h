#ifndef SETTINGS_H
#define SETTINGS_H

#include <Arduino.h>
#include <ArduinoJson.h>
#include "../transit/transit_types.h"

// Settings stored in NVS. They survive a restart and reflashing.
struct AppSettings {
    char          wifiSsid[33];
    char          wifiPass[65];
    MonitorConfig monitors[kMaxMonitors];
    uint8_t       nMonitors;
    uint8_t       brightness;   // 10..255
    bool          flip;         // rotate display by 180°
};

namespace settings {

void load(AppSettings &s);
void saveWifi(const char *ssid, const char *pass);
void clearWifi();
void saveMonitors(const AppSettings &s);
void saveDisplay(const AppSettings &s);

// Default lines (Döbling: S45, 38, 35A, 10A)
void loadDefaultMonitors(AppSettings &s);

// JSON conversion, used for NVS and the web API
void monitorsToJson(const AppSettings &s, JsonArray arr);
// Returns false if an entry is invalid
bool monitorsFromJson(JsonArrayConst arr, AppSettings &s, String *error);

} // namespace settings

#endif
