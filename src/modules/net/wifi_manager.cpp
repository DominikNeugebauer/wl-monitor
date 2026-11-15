#include "wifi_manager.h"
#include "../logger/logger.h"
#include "../../config.h"
#include <ESPmDNS.h>

static void wlog(const String &s, LogLevel l = INFO) { Logger::getInstance()->log(s.c_str(), l); }

void WifiManager::begin(const char *ssid, const char *pass) {
    _ssid = ssid ? ssid : "";
    _pass = pass ? pass : "";

    uint8_t mac[6];
    WiFi.macAddress(mac);
    char buf[32];
    snprintf(buf, sizeof(buf), "%s%02X%02X", AP_SSID_PREFIX, mac[4], mac[5]);
    _apSsid = buf;

    WiFi.persistent(false);          // credentials are stored in our own NVS namespace
    WiFi.setHostname(APP_HOSTNAME);
    WiFi.setAutoReconnect(true);
    WiFi.onEvent([this](WiFiEvent_t, WiFiEventInfo_t info) {
        _lastReason = info.wifi_sta_disconnected.reason;
    }, ARDUINO_EVENT_WIFI_STA_DISCONNECTED);

    if (hasCredentials()) {
        WiFi.mode(WIFI_STA);
        connect(_ssid, _pass, false);
    } else {
        startAp();
    }
}

void WifiManager::connect(const String &ssid, const String &pass, bool fromPortal) {
    _pendingSsid = ssid;
    _pendingPass = pass;
    _fromPortal = fromPortal;
    _state = StaState::Connecting;
    _connectStart = millis();
    _lastRetry = millis();
    _lastReason = 0;

    wlog("WiFi: connecting to " + ssid);
    WiFi.mode(_apActive ? WIFI_AP_STA : WIFI_STA);
    // Power save off, otherwise incoming requests and mDNS get lost
    WiFi.setSleep(false);
    WiFi.disconnect(false);
    delay(50);
    WiFi.begin(ssid.c_str(), pass.c_str());
}

void WifiManager::forget() {
    _ssid = "";
    _pass = "";
    WiFi.disconnect(false);
    _state = StaState::Idle;
    startAp();
}

const char *WifiManager::stateName() const {
    switch (_state) {
        case StaState::Connecting: return "connecting";
        case StaState::Connected: return "connected";
        case StaState::Failed: return "failed";
        default: return "idle";
    }
}

void WifiManager::startAp() {
    if (_apActive) return;
    wlog("WiFi: starting setup AP " + _apSsid);
    WiFi.mode(WIFI_AP_STA);
    WiFi.softAPConfig(_apIp, _apIp, IPAddress(255, 255, 255, 0));
    const char *pw = strlen(AP_PASSWORD) >= 8 ? AP_PASSWORD : nullptr;
    WiFi.softAP(_apSsid.c_str(), pw);
    // Captive portal: answer every DNS request with our own IP
    _dns.setErrorReplyCode(DNSReplyCode::NoError);
    _dns.start(53, "*", _apIp);
    _apActive = true;
}

void WifiManager::stopAp() {
    if (!_apActive) return;
    wlog("WiFi: setup AP stopped");
    _dns.stop();
    WiFi.softAPdisconnect(true);
    WiFi.mode(WIFI_STA);
    _apActive = false;
}

void WifiManager::onGotIp() {
    _state = StaState::Connected;
    _connectedSince = millis();
    wlog("WiFi: connected, IP " + WiFi.localIP().toString());

    if (_fromPortal || _ssid != _pendingSsid || _pass != _pendingPass) {
        if (_pendingSsid.length()) {
            _ssid = _pendingSsid;
            _pass = _pendingPass;
            if (_cb) _cb(_ssid, _pass); // save to NVS
        }
    }
    _fromPortal = false;

    // Restart mDNS on every reconnect, it stops answering after a WiFi drop
    if (_mdnsStarted) MDNS.end();
    _mdnsStarted = MDNS.begin(APP_HOSTNAME);
    if (_mdnsStarted) MDNS.addService("http", "tcp", 80);
    else wlog("mDNS failed to start", WARNING);
}

void WifiManager::loop() {
    if (_apActive) _dns.processNextRequest();

    const uint32_t now = millis();
    const bool up = WiFi.status() == WL_CONNECTED;

    switch (_state) {
        case StaState::Connecting: {
            if (up) { onGotIp(); break; }
            uint8_t r = _lastReason;
            bool authFail = (r == 15 || r == 202 || r == 204 || r == 2); // wrong password
            bool notFound = (r == 201);
            uint32_t dt = now - _connectStart;
            if (dt > STA_CONNECT_TIMEOUT_MS || ((authFail || notFound) && dt > 6000)) {
                wlog("WiFi: connection failed, reason " + String(r), WARNING);
                _state = StaState::Failed;
                _lostSince = _lostSince ? _lostSince : now;
                WiFi.disconnect(false); // keep the AP channel stable
                if (!_apActive && (!hasCredentials() || _fromPortal || !_connectedSince)) startAp();
                _fromPortal = false;
                _lastRetry = now;
            }
            break;
        }
        case StaState::Connected:
            if (!up) {
                wlog("WiFi: connection lost", WARNING);
                _state = StaState::Idle;
                _lostSince = now;
                _lastRetry = now;
                break;
            }
            // Keep the setup AP open for a while after setup so the phone can show the result.
            // Stays open for up to 15 min while a client is still connected.
            if (_apActive && now - _connectedSince > AP_LINGER_MS &&
                (apClients() == 0 || now - _connectedSince > 15UL * 60UL * 1000UL))
                stopAp();
            break;

        case StaState::Idle:
        case StaState::Failed:
            if (up) { onGotIp(); break; }
            if (!hasCredentials()) {
                if (!_apActive) startAp();
                break;
            }
            // Home WiFi gone for too long, offer the setup AP again
            if (!_apActive && _lostSince && now - _lostSince > AP_FALLBACK_AFTER_MS) startAp();
            // Retry periodically, but not while a client is on the setup AP
            // because scanning changes the channel
            if (now - _lastRetry > STA_RETRY_INTERVAL_MS && apClients() == 0) {
                connect(_ssid, _pass, false);
            }
            break;
    }
}
