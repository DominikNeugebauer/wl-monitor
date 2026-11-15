#ifndef WIFI_MANAGER_H
#define WIFI_MANAGER_H

#include <Arduino.h>
#include <WiFi.h>
#include <DNSServer.h>
#include <functional>

// WiFi handling.
// Connects as a station with the saved credentials and reconnects automatically.
// Opens a setup access point with a captive portal when no WiFi is configured
// or the home network is gone for a while. AP and station can run at the same time.
enum class StaState : uint8_t { Idle, Connecting, Connected, Failed };

class WifiManager {
public:
    using ProvisionedCb = std::function<void(const String &ssid, const String &pass)>;

    void begin(const char *ssid, const char *pass);
    void loop();

    // Start a new connection attempt, e.g. from the web form
    void connect(const String &ssid, const String &pass, bool fromPortal);
    void forget();                 // clear credentials and start the setup AP
    void onProvisioned(ProvisionedCb cb) { _cb = cb; }

    bool      staConnected() const { return WiFi.status() == WL_CONNECTED; }
    StaState  state() const { return _state; }
    const char *stateName() const;
    uint8_t   lastReason() const { return _lastReason; }
    bool      portalAttempt() const { return _fromPortal; }
    bool      hasCredentials() const { return _ssid.length() > 0; }
    const String &ssid() const { return _ssid; }
    const String &pendingSsid() const { return _pendingSsid; }

    bool      apActive() const { return _apActive; }
    const String &apSsid() const { return _apSsid; }
    IPAddress apIp() const { return _apIp; }
    uint8_t   apClients() const { return _apActive ? WiFi.softAPgetStationNum() : 0; }

    // True while the first connection after boot is in progress
    bool bootConnecting() const { return _state == StaState::Connecting && !_apActive; }

    void startAp();
    void stopAp();

private:
    void onGotIp();

    DNSServer   _dns;
    IPAddress   _apIp{192, 168, 4, 1};
    String      _apSsid;
    bool        _apActive = false;

    String      _ssid, _pass;               // saved credentials
    String      _pendingSsid, _pendingPass; // credentials being tried
    bool        _fromPortal = false;
    StaState    _state = StaState::Idle;
    uint32_t    _connectStart = 0;
    uint32_t    _connectedSince = 0;
    uint32_t    _lostSince = 0;
    uint32_t    _lastRetry = 0;
    volatile uint8_t _lastReason = 0;
    bool        _mdnsStarted = false;
    ProvisionedCb _cb;
};

#endif
