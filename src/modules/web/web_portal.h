#ifndef WEB_PORTAL_H
#define WEB_PORTAL_H

#include <Arduino.h>
#include <WebServer.h>
#include <ArduinoJson.h>
#include <functional>
#include "../net/wifi_manager.h"
#include "../settings/settings.h"
#include "../transit/transit_service.h"

// Web interface and REST API on port 80
//   /                 web app from web/index.html, gzipped in flash
//   /stops.txt        Wiener Linien stop list (DIVA;name)
//   /api/status       WiFi and system status
//   /api/scan         nearby WiFi networks
//   /api/wifi         POST {ssid, pass}, connect and save
//   /api/wifi/forget  POST, clear WiFi credentials
//   /api/config       GET/POST lines and display settings
//   /api/config/reset POST, restore default lines
//   /api/departures   current departures, same as on the display
//   /api/wl/lookup    ?diva=  lines and directions of a WL station
//   /api/oebb/search  ?q=     ÖBB station search
//   /api/oebb/lines   ?id=    lines and destinations of an ÖBB station
//   /api/restart      POST
class WebPortal {
public:
    using Callback = std::function<void()>;

    void begin(WifiManager &wm, AppSettings &settings, TransitService &transit, Callback onDisplayChanged);
    void loop();

private:
    void sendJson(int code, JsonDocument &doc);
    void sendError(int code, const String &msg);
    bool parseBody(JsonDocument &doc);
    bool isOwnHost(const String &host);
    bool requireInternet();

    void handleStatus();
    void handleScan();
    void handleWifi();
    void handleForget();
    void handleGetConfig();
    void handlePostConfig();
    void handleResetConfig();
    void handleDepartures();
    void handleWlLookup();
    void handleOebbSearch();
    void handleOebbLines();
    void handleNotFound();

    WebServer       _server{80};
    WifiManager    *_wm = nullptr;
    AppSettings    *_settings = nullptr;
    TransitService *_transit = nullptr;
    Callback        _onDisplay;

    String   _scanCache = "[]";
    uint32_t _scanAt = 0;
    uint32_t _restartAt = 0;
};

#endif
