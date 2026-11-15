#include "web_portal.h"
#include "../transit/transit_api.h"
#include "../transit/time_util.h"
#include "../logger/logger.h"
#include "../../config.h"
#include "../../generated/web_assets.h"

void WebPortal::begin(WifiManager &wm, AppSettings &settings, TransitService &transit, Callback onDisplayChanged) {
    _wm = &wm;
    _settings = &settings;
    _transit = &transit;
    _onDisplay = onDisplayChanged;

    _server.on("/", HTTP_GET, [this]() {
        _server.sendHeader("Cache-Control", "no-cache");
        _server.sendHeader("Content-Encoding", "gzip");
        _server.send_P(200, "text/html; charset=utf-8", (const char *)index_html_gz, index_html_gz_len);
    });
    _server.on("/stops.txt", HTTP_GET, [this]() {
        _server.sendHeader("Cache-Control", "max-age=86400");
        _server.sendHeader("Content-Encoding", "gzip");
        _server.send_P(200, "text/plain; charset=utf-8", (const char *)stops_txt_gz, stops_txt_gz_len);
    });
    _server.on("/favicon.ico", HTTP_GET, [this]() { _server.send(204); });
    _server.on("/api/status", HTTP_GET, [this]() { handleStatus(); });
    _server.on("/api/scan", HTTP_GET, [this]() { handleScan(); });
    _server.on("/api/wifi", HTTP_POST, [this]() { handleWifi(); });
    _server.on("/api/wifi/forget", HTTP_POST, [this]() { handleForget(); });
    _server.on("/api/config", HTTP_GET, [this]() { handleGetConfig(); });
    _server.on("/api/config", HTTP_POST, [this]() { handlePostConfig(); });
    _server.on("/api/config/reset", HTTP_POST, [this]() { handleResetConfig(); });
    _server.on("/api/departures", HTTP_GET, [this]() { handleDepartures(); });
    _server.on("/api/wl/lookup", HTTP_GET, [this]() { handleWlLookup(); });
    _server.on("/api/oebb/search", HTTP_GET, [this]() { handleOebbSearch(); });
    _server.on("/api/oebb/lines", HTTP_GET, [this]() { handleOebbLines(); });
    _server.on("/api/restart", HTTP_POST, [this]() {
        JsonDocument d;
        d["ok"] = true;
        sendJson(200, d);
        _restartAt = millis() + 800;
    });
    _server.onNotFound([this]() { handleNotFound(); });
    _server.begin();
}

void WebPortal::loop() {
    _server.handleClient();
    if (_restartAt && (int32_t)(millis() - _restartAt) > 0) ESP.restart();
}

// Helpers

void WebPortal::sendJson(int code, JsonDocument &doc) {
    String out;
    serializeJson(doc, out);
    _server.sendHeader("Cache-Control", "no-store");
    _server.send(code, "application/json; charset=utf-8", out);
}

void WebPortal::sendError(int code, const String &msg) {
    JsonDocument d;
    d["ok"] = false;
    d["error"] = msg;
    sendJson(code, d);
}

bool WebPortal::parseBody(JsonDocument &doc) {
    if (!_server.hasArg("plain")) {
        sendError(400, "Kein Inhalt");
        return false;
    }
    DeserializationError e = deserializeJson(doc, _server.arg("plain"));
    if (e) {
        sendError(400, String("Ungültiges JSON: ") + e.c_str());
        return false;
    }
    return true;
}

bool WebPortal::isOwnHost(const String &host) {
    if (host.length() == 0) return true;
    if (host == _wm->apIp().toString()) return true;
    if (_wm->staConnected() && host == WiFi.localIP().toString()) return true;
    if (host.startsWith(APP_HOSTNAME)) return true;
    return false;
}

bool WebPortal::requireInternet() {
    if (_wm->staConnected()) return true;
    sendError(503, "Noch nicht mit dem Internet verbunden - bitte zuerst WLAN einrichten.");
    return false;
}

void WebPortal::handleNotFound() {
    // Captive portal: redirect connectivity checks (captive.apple.com etc.) to our page
    if (_wm->apActive() && !isOwnHost(_server.hostHeader())) {
        _server.sendHeader("Location", String("http://") + _wm->apIp().toString() + "/", true);
        _server.send(302, "text/plain", "");
        return;
    }
    _server.send(404, "text/plain", "Nicht gefunden");
}

// API

void WebPortal::handleStatus() {
    JsonDocument d;
    JsonObject ap = d["ap"].to<JsonObject>();
    ap["active"] = _wm->apActive();
    ap["ssid"] = _wm->apSsid();
    ap["ip"] = _wm->apIp().toString();
    ap["clients"] = _wm->apClients();

    JsonObject sta = d["sta"].to<JsonObject>();
    sta["configured"] = _wm->hasCredentials();
    sta["ssid"] = _wm->ssid();
    sta["pending"] = _wm->pendingSsid();
    sta["state"] = _wm->stateName();
    sta["connected"] = _wm->staConnected();
    sta["reason"] = _wm->lastReason();
    if (_wm->staConnected()) {
        sta["ip"] = WiFi.localIP().toString();
        sta["rssi"] = WiFi.RSSI();
    }

    d["host"] = String(APP_HOSTNAME) + ".local";
    d["timeOk"] = timeutil::timeValid(time(nullptr));
    d["version"] = FW_VERSION;
    d["heap"] = ESP.getFreeHeap();
    d["uptime"] = millis() / 1000;
    JsonObject f = d["fetch"].to<JsonObject>();
    uint32_t last = _transit->lastFetchMs();
    f["ago"] = last ? (int32_t)((millis() - last) / 1000) : -1;
    f["error"] = _transit->lastError();
    f["fallback"] = _transit->fallbackActive();
    f["errorText"] = _transit->lastErrorText();
    f["interval"] = FETCH_INTERVAL_MS / 1000;
    sendJson(200, d);
}

void WebPortal::handleScan() {
    int16_t n = WiFi.scanComplete();
    if (n >= 0) {
        JsonDocument d;
        JsonArray arr = d.to<JsonArray>();
        for (int16_t i = 0; i < n; i++) {
            String ssid = WiFi.SSID(i);
            if (!ssid.length()) continue;
            bool dup = false;
            for (JsonObject o : arr) if (ssid == (o["ssid"] | "")) dup = true; // list is sorted, strongest first
            if (dup) continue;
            JsonObject o = arr.add<JsonObject>();
            o["ssid"] = ssid;
            o["rssi"] = WiFi.RSSI(i);
            o["secure"] = WiFi.encryptionType(i) != WIFI_AUTH_OPEN;
        }
        _scanCache = "";
        serializeJson(d, _scanCache);
        _scanAt = millis();
        WiFi.scanDelete();
    }
    bool scanning = (n == WIFI_SCAN_RUNNING);
    if (n == WIFI_SCAN_FAILED && (_scanAt == 0 || millis() - _scanAt > 8000)) {
        WiFi.scanNetworks(true, false);
        scanning = true;
    }
    String out = String("{\"scanning\":") + (scanning ? "true" : "false") + ",\"networks\":" + _scanCache + "}";
    _server.sendHeader("Cache-Control", "no-store");
    _server.send(200, "application/json; charset=utf-8", out);
}

void WebPortal::handleWifi() {
    JsonDocument body;
    if (!parseBody(body)) return;
    String ssid = body["ssid"] | "";
    String pass = body["pass"] | "";
    if (!ssid.length() || ssid.length() > 32 || pass.length() > 64) {
        sendError(400, "SSID/Passwort ungültig");
        return;
    }
    JsonDocument d;
    d["ok"] = true;
    sendJson(200, d);
    // Connect after the response is sent, the channel change can drop the AP client
    delay(100);
    _wm->connect(ssid, pass, true);
}

void WebPortal::handleForget() {
    settings::clearWifi();
    _settings->wifiSsid[0] = 0;
    _settings->wifiPass[0] = 0;
    JsonDocument d;
    d["ok"] = true;
    sendJson(200, d);
    delay(100);
    _wm->forget();
}

void WebPortal::handleGetConfig() {
    JsonDocument d;
    settings::monitorsToJson(*_settings, d["monitors"].to<JsonArray>());
    d["brightness"] = _settings->brightness;
    d["flip"] = _settings->flip;
    d["max"] = kMaxMonitors;
    sendJson(200, d);
}

void WebPortal::handlePostConfig() {
    JsonDocument body;
    if (!parseBody(body)) return;

    if (body["monitors"].is<JsonArrayConst>()) {
        String err;
        if (!settings::monitorsFromJson(body["monitors"].as<JsonArrayConst>(), *_settings, &err)) {
            sendError(400, err);
            return;
        }
        settings::saveMonitors(*_settings);
        _transit->setConfig(_settings->monitors, _settings->nMonitors);
    }
    bool displayChanged = false;
    if (body["brightness"].is<int>()) {
        int b = body["brightness"].as<int>();
        _settings->brightness = (uint8_t)constrain(b, 10, 255);
        displayChanged = true;
    }
    if (body["flip"].is<bool>()) {
        _settings->flip = body["flip"].as<bool>();
        displayChanged = true;
    }
    if (displayChanged) {
        settings::saveDisplay(*_settings);
        if (_onDisplay) _onDisplay();
    }
    handleGetConfig();
}

void WebPortal::handleResetConfig() {
    settings::loadDefaultMonitors(*_settings);
    settings::saveMonitors(*_settings);
    _transit->setConfig(_settings->monitors, _settings->nMonitors);
    handleGetConfig();
}

void WebPortal::handleDepartures() {
    static Row rows[kMaxMonitors];
    size_t n = _transit->snapshot(rows, kMaxMonitors);
    time_t now = time(nullptr);
    bool tOk = timeutil::timeValid(now);

    JsonDocument d;
    d["now"] = (uint32_t)now;
    d["timeOk"] = tOk;
    JsonArray arr = d["rows"].to<JsonArray>();
    for (size_t i = 0; i < n; i++) {
        const Row &r = rows[i];
        JsonObject o = arr.add<JsonObject>();
        o["line"] = r.line;
        o["dest"] = r.dest;
        o["stop"] = r.stopName;
        o["kind"] = transit::kindName(r.kind);
        o["src"] = r.src == Source::WL ? "wl" : "oebb";
        o["ok"] = r.ok;
        if (r.viaFallback) o["fallback"] = true;
        JsonArray deps = o["deps"].to<JsonArray>();
        for (int k = 0; k < r.nDeps; k++) {
            const Departure &dep = r.deps[k];
            if (tOk && dep.real < now - 30) continue;
            JsonObject j = deps.add<JsonObject>();
            long secs = (long)(dep.real - now);
            j["min"] = secs <= 0 ? 0 : secs / 60;
            long dl = (long)(dep.real - dep.planned);
            j["delay"] = (dl >= 0 ? dl + 30 : dl - 30) / 60;
            j["rt"] = dep.realtime;
            if (dep.cancelled) j["cancelled"] = true;
            struct tm lt;
            time_t t = dep.real;
            localtime_r(&t, &lt);
            char hm[6];
            snprintf(hm, sizeof(hm), "%02d:%02d", lt.tm_hour, lt.tm_min);
            j["time"] = hm;
        }
    }
    sendJson(200, d);
}

void WebPortal::handleWlLookup() {
    if (!requireInternet()) return;
    uint32_t diva = strtoul(_server.arg("diva").c_str(), nullptr, 10);
    if (!diva) { sendError(400, "diva fehlt"); return; }
    JsonDocument d;
    String err;
    if (!transit::wlLookup(diva, d.to<JsonArray>(), err)) { sendError(502, "Wiener Linien: " + err); return; }
    sendJson(200, d);
}

void WebPortal::handleOebbSearch() {
    if (!requireInternet()) return;
    String q = _server.arg("q");
    q.trim();
    if (q.length() < 2) { sendError(400, "Suchbegriff zu kurz"); return; }
    JsonDocument d;
    String err;
    if (!transit::oebbSearch(q, d.to<JsonArray>(), err)) { sendError(502, "\xC3\x96" "BB: " + err); return; }
    sendJson(200, d);
}

void WebPortal::handleOebbLines() {
    if (!requireInternet()) return;
    uint32_t id = strtoul(_server.arg("id").c_str(), nullptr, 10);
    if (!id) { sendError(400, "id fehlt"); return; }
    JsonDocument d;
    String err;
    if (!transit::oebbLines(id, d.to<JsonArray>(), err)) { sendError(502, "\xC3\x96" "BB: " + err); return; }
    sendJson(200, d);
}
