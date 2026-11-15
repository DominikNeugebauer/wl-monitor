#include "transit_service.h"
#include "transit_api.h"
#include "transit_parse.h"
#include "time_util.h"
#include "text_util.h"
#include "../logger/logger.h"
#include "../../config.h"
#include <WiFi.h>

void TransitService::begin() {
    _mtx = xSemaphoreCreateMutex();
    // 16 KB stack for TLS, runs on core 0 (Arduino loop is on core 1)
    xTaskCreatePinnedToCore(taskEntry, "transit", 16384, this, 1, &_task, 0);
}

void TransitService::setConfig(const MonitorConfig *cfgs, uint8_t n) {
    xSemaphoreTake(_mtx, portMAX_DELAY);
    _n = n > kMaxMonitors ? kMaxMonitors : n;
    for (uint8_t i = 0; i < _n; i++) {
        _cfg[i] = cfgs[i];
        transit::initRow(_rows[i], _cfg[i]);
    }
    _cfgVersion++;
    xSemaphoreGive(_mtx);
    requestRefresh();
}

void TransitService::requestRefresh() {
    if (_task) xTaskNotifyGive(_task);
}

size_t TransitService::snapshot(Row *out, size_t max) {
    xSemaphoreTake(_mtx, portMAX_DELAY);
    size_t n = _n < max ? _n : max;
    memcpy(out, _rows, n * sizeof(Row));
    xSemaphoreGive(_mtx);
    return n;
}

String TransitService::lastErrorText() {
    xSemaphoreTake(_mtx, portMAX_DELAY);
    String s = _errText;
    xSemaphoreGive(_mtx);
    return s;
}

float TransitService::progress() const {
    if (_lastFetchMs == 0) return 0;
    float p = (float)(millis() - _lastFetchMs) / (float)FETCH_INTERVAL_MS;
    return p > 1 ? 1 : p;
}

void TransitService::taskEntry(void *arg) {
    static_cast<TransitService *>(arg)->run();
}

void TransitService::run() {
    for (;;) {
        // Wait for WiFi and time sync
        if (WiFi.status() != WL_CONNECTED || !timeutil::timeValid(time(nullptr))) {
            ulTaskNotifyTake(pdTRUE, pdMS_TO_TICKS(1000));
            continue;
        }
        fetchAll();
        // Sleep until the next interval or until requestRefresh()
        uint32_t elapsed = millis() - _lastFetchMs;
        uint32_t wait = elapsed >= FETCH_INTERVAL_MS ? 0 : FETCH_INTERVAL_MS - elapsed;
        ulTaskNotifyTake(pdTRUE, pdMS_TO_TICKS(wait));
    }
}

void TransitService::fetchAll() {
    // Local copy of the config
    MonitorConfig cfg[kMaxMonitors];
    Row rows[kMaxMonitors];
    xSemaphoreTake(_mtx, portMAX_DELAY);
    uint8_t n = _n;
    uint32_t version = _cfgVersion;
    memcpy(cfg, _cfg, sizeof(MonitorConfig) * n);
    memcpy(rows, _rows, sizeof(Row) * n);
    xSemaphoreGive(_mtx);

    _fetching = true;
    time_t now = time(nullptr);
    bool anyError = false;
    String errText;

    // Fallback data belongs to one config version
    if (_fbVersion != version) {
        memset(_fbEva, 0, sizeof(_fbEva));
        memset(_fbTriedMs, 0, sizeof(_fbTriedMs));
        memset(_learnedDest, 0, sizeof(_learnedDest));
        _fbVersion = version;
        _wlBackoff = false; // new config, try WL again right away
    }

    // Wiener Linien: one request for all stops
    bool hasWl = false;
    for (uint8_t i = 0; i < n; i++) if (cfg[i].src == Source::WL) hasWl = true;
    if (hasWl) {
        bool wlOk = false;
        String wlErr;
        if (!_wlBackoff || (int32_t)(millis() - _wlRetryAt) >= 0) {
            wlOk = transit::fetchWl(cfg, n, rows, now, wlErr);
            if (wlOk) {
                if (_wlBackoff) Logger::getInstance()->log("Wiener Linien reachable again");
                _wlBackoff = false;
                // Remember destinations for a later fallback
                for (uint8_t i = 0; i < n; i++) {
                    if (cfg[i].src != Source::WL || rows[i].nDeps == 0) continue;
                    text::copy(_learnedDest[i], sizeof(_learnedDest[i]), rows[i].dest);
                }
            } else {
                _wlBackoff = true;
                _wlRetryAt = millis() + WL_RETRY_INTERVAL_MS;
                Logger::getInstance()->log(("Wiener Linien: " + wlErr + " -> \xC3\x96" "BB-Fallback").c_str(), WARNING);
            }
        }
        _fallbackActive = !wlOk;
        if (!wlOk) {
            errText = "Wiener Linien not reachable, using \xC3\x96" "BB data";
            if (wlErr.length()) errText += " (" + wlErr + ")";
            fetchWlViaOebb(cfg, rows, n, now, anyError, errText);
        }
    } else {
        _fallbackActive = false;
    }

    // ÖBB: one request per entry
    for (uint8_t i = 0; i < n; i++) {
        if (cfg[i].src != Source::OEBB) continue;
        String err;
        if (!transit::fetchOebb(cfg[i], rows[i], now, err)) {
            anyError = true;
            errText = "\xC3\x96" "BB: " + err;
            rows[i].ok = false;
            Logger::getInstance()->log(errText.c_str(), WARNING);
        }
    }

    // Store the result unless the config changed in the meantime
    xSemaphoreTake(_mtx, portMAX_DELAY);
    if (version == _cfgVersion) memcpy(_rows, rows, sizeof(Row) * n);
    _errText = errText;
    xSemaphoreGive(_mtx);

    _lastError = anyError;
    _lastFetchMs = millis();
    _fetching = false;

    char msg[64];
    snprintf(msg, sizeof(msg), "Departures updated (%u rows, heap %u)", n, (unsigned)ESP.getFreeHeap());
    Logger::getInstance()->log(msg, anyError ? WARNING : INFO);
}

// Fills WL rows from the ÖBB departure board of the matching stop
void TransitService::fetchWlViaOebb(const MonitorConfig *cfg, Row *rows, uint8_t n, time_t now,
                                    bool &anyError, String &errText) {
    for (uint8_t i = 0; i < n; i++) {
        if (cfg[i].src != Source::WL) continue;
        String err;

        // Look up the ÖBB station once and cache it
        if (!_fbEva[i]) {
            if (_fbTriedMs[i] && millis() - _fbTriedMs[i] < FALLBACK_RESOLVE_RETRY_MS) {
                rows[i].ok = false;
                anyError = true;
                continue;
            }
            uint32_t eva = 0;
            if (transit::oebbFindStopForLine(cfg[i].stopName, cfg[i].line, eva, err)) {
                _fbEva[i] = eva;
                char msg[80];
                snprintf(msg, sizeof(msg), "Fallback: %s %s -> \xC3\x96" "BB-Station %lu", cfg[i].line,
                         cfg[i].stopName, (unsigned long)eva);
                Logger::getInstance()->log(msg);
            } else {
                _fbTriedMs[i] = millis() ? millis() : 1;
                rows[i].ok = false;
                anyError = true;
                errText += "; " + String(cfg[i].line) + ": " + err;
                Logger::getInstance()->log(("Fallback " + String(cfg[i].line) + ": " + err).c_str(), WARNING);
                continue;
            }
        }

        // Direction: saved destination, else the last one from WL, else all directions
        const char *dest = cfg[i].dest[0] ? cfg[i].dest : _learnedDest[i];
        char key[24];
        text::keyword(dest, key, sizeof(key));

        if (!transit::fetchOebbFallback(cfg[i], _fbEva[i], key, rows[i], now, err)) {
            rows[i].ok = false;
            anyError = true;
            errText += "; " + String(cfg[i].line) + ": " + err;
            Logger::getInstance()->log(("Fallback " + String(cfg[i].line) + ": " + err).c_str(), WARNING);
        }
    }
}
