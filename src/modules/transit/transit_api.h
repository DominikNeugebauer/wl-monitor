#ifndef TRANSIT_API_H
#define TRANSIT_API_H

#include <Arduino.h>
#include <ArduinoJson.h>
#include "transit_types.h"

// Requests to Wiener Linien OGD Realtime and ÖBB Scotty
namespace transit {

// Loads all WL entries in one request
bool fetchWl(const MonitorConfig *cfgs, size_t n, Row *rows, time_t now, String &error);
// Loads one ÖBB entry (station and optional direction)
bool fetchOebb(const MonitorConfig &cfg, Row &row, time_t now, String &error);

// Used by the web interface

// Lines and directions of a WL station (DIVA) as [{rbl,line,towards,type}]
bool wlLookup(uint32_t diva, JsonArray out, String &error);
// ÖBB station search as [{id,name}]
bool oebbSearch(const String &query, JsonArray out, String &error);
// Unique lines on an ÖBB departure board as [{line,dest,kind}]
bool oebbLines(uint32_t eva, JsonArray out, String &error);

// Fallback when wienerlinien.at is not reachable

// Finds the ÖBB station for a WL stop name where the line also stops
bool oebbFindStopForLine(const char *wlStopName, const char *line, uint32_t &eva, String &error);
// Departures of a WL line from the ÖBB board. destKey filters the destination, empty for all.
bool fetchOebbFallback(const MonitorConfig &wlCfg, uint32_t eva, const char *destKey, Row &row,
                       time_t now, String &error);

const char *kindName(VehicleKind k);

} // namespace transit

#endif
