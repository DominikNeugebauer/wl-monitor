#ifndef TRANSIT_TYPES_H
#define TRANSIT_TYPES_H

#include <stdint.h>
#include <stddef.h>
#include <time.h>

// Data types for the departure board

enum class Source : uint8_t { WL = 0, OEBB = 1 };

enum class VehicleKind : uint8_t { Tram, Bus, NightBus, Metro, SBahn, Train, Other };

// One configured entry, shown as one row on the display
struct MonitorConfig {
    Source   src;
    uint32_t stop;          // WL: RBL (stop ID), ÖBB: station evaId
    uint32_t dir;           // ÖBB only: evaId of a station in the travel direction, 0 for any
    char     line[8];       // e.g. "38", "35A", "S45"
    char     dest[24];      // ÖBB: destination filter (substring)
                            // WL: destination, only used for the ÖBB fallback
    char     stopName[32];  // shown in the web UI
};

static const int kMaxDepsPerRow = 4;
static const int kMaxMonitors   = 10;

struct Departure {
    time_t planned;     // scheduled time (UTC epoch)
    time_t real;        // real-time estimate, same as planned if there is none
    bool   realtime;    // real-time data available
    bool   cancelled;   // cancelled
};

struct Row {
    char        line[8];
    char        dest[32];
    char        stopName[32];
    VehicleKind kind;
    Source      src;
    Departure   deps[kMaxDepsPerRow];
    uint8_t     nDeps;
    bool        ok;         // last fetch succeeded
    bool        viaFallback;// data comes from ÖBB because WL is not reachable
    time_t      fetchedAt;  // time of the last successful fetch
};

#endif
