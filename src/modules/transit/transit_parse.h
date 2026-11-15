#ifndef TRANSIT_PARSE_H
#define TRANSIT_PARSE_H

#include <ArduinoJson.h>
#include "transit_types.h"

// Parsers for Wiener Linien OGD Realtime and the ÖBB Scotty live ticker.
// No WiFi or HTTP code in here.
namespace transit {

// Fills a row with defaults from the config before the first fetch
void initRow(Row &row, const MonitorConfig &cfg);

// Wiener Linien

// Filter that keeps only the needed fields of the response
void buildWlFilter(JsonDocument &filter);
// Fills rows[i] for every cfgs[i] with source WL. Returns the number of lines found.
int parseWl(JsonDocument &doc, const MonitorConfig *cfgs, size_t n, Row *rows, time_t now);
VehicleKind kindFromWlType(const char *type, const char *line);

// ÖBB
void buildOebbFilter(JsonDocument &filter);
bool parseOebb(JsonDocument &doc, const MonitorConfig &cfg, Row &row, time_t now);
// "S 45" gives S45/SBahn, "Bus 35A" gives 35A/Bus, "RJX 766" gives RJX/Train
VehicleKind parseOebbProduct(const char *pr, char *lineOut, size_t n);

} // namespace transit

#endif
