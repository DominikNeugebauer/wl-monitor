#ifndef TRANSIT_SERVICE_H
#define TRANSIT_SERVICE_H

#include <Arduino.h>
#include "transit_types.h"

// Fetches all departures every minute in a FreeRTOS task.
// The UI only reads snapshots, so it does not block during HTTPS requests.
class TransitService {
public:
    void begin();
    void setConfig(const MonitorConfig *cfgs, uint8_t n); // copies the config and refreshes right away
    void requestRefresh();

    // Copies the current rows and returns the count
    size_t snapshot(Row *out, size_t max);

    bool     fetching() const { return _fetching; }
    bool     lastError() const { return _lastError; }
    String   lastErrorText();
    uint32_t lastFetchMs() const { return _lastFetchMs; }
    float    progress() const; // 0..1 until the next fetch
    bool     fallbackActive() const { return _fallbackActive; } // WL rows currently come from ÖBB

private:
    static void taskEntry(void *arg);
    void run();
    void fetchAll();

    SemaphoreHandle_t _mtx = nullptr;
    TaskHandle_t      _task = nullptr;
    MonitorConfig     _cfg[kMaxMonitors];
    Row               _rows[kMaxMonitors];
    uint8_t           _n = 0;
    uint32_t          _cfgVersion = 0;
    volatile bool     _fetching = false;
    volatile bool     _lastError = false;
    volatile uint32_t _lastFetchMs = 0;
    String            _errText;

    // ÖBB fallback for WL rows, only used inside the task
    void fetchWlViaOebb(const MonitorConfig *cfg, Row *rows, uint8_t n, time_t now,
                        bool &anyError, String &errText);
    uint32_t          _fbVersion = 0xFFFFFFFF;          // config version the fallback data belongs to
    uint32_t          _fbEva[kMaxMonitors];             // ÖBB station per row, 0 if unknown
    uint32_t          _fbTriedMs[kMaxMonitors];         // last failed lookup
    char              _learnedDest[kMaxMonitors][24];   // destination from the last WL response
    uint32_t          _wlRetryAt = 0;                   // do not retry WL before this time
    bool              _wlBackoff = false;
    volatile bool     _fallbackActive = false;
};

#endif
