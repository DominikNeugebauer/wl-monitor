#include "transit_parse.h"
#include "text_util.h"
#include "time_util.h"
#include <string.h>
#include <ctype.h>
#include <stdlib.h>
#include <stdio.h>

namespace transit {

void initRow(Row &row, const MonitorConfig &cfg) {
    memset(&row, 0, sizeof(row));
    text::copy(row.line, sizeof(row.line), cfg.line);
    text::copy(row.stopName, sizeof(row.stopName), cfg.stopName);
    // Show the station name until the destination is known
    text::copy(row.dest, sizeof(row.dest), cfg.stopName);
    row.src = cfg.src;
    row.kind = (cfg.src == Source::OEBB)
                   ? ((toupper((unsigned char)cfg.line[0]) == 'S') ? VehicleKind::SBahn : VehicleKind::Train)
                   : kindFromWlType(nullptr, cfg.line);
    row.ok = false;
}

// Inserts sorted by time and keeps at most kMaxDepsPerRow
static void addDeparture(Row &row, const Departure &d, time_t now) {
    if (timeutil::timeValid(now) && d.real < now - 30) return; // already gone
    int pos = row.nDeps;
    while (pos > 0 && row.deps[pos - 1].real > d.real) pos--;
    if (pos >= kMaxDepsPerRow) return;
    int last = row.nDeps < kMaxDepsPerRow ? row.nDeps : kMaxDepsPerRow - 1;
    for (int i = last; i > pos; i--) row.deps[i] = row.deps[i - 1];
    row.deps[pos] = d;
    if (row.nDeps < kMaxDepsPerRow) row.nDeps++;
}

// Wiener Linien

VehicleKind kindFromWlType(const char *type, const char *line) {
    if (type) {
        if (!strcmp(type, "ptTram") || !strcmp(type, "ptTramWLB")) return VehicleKind::Tram;
        if (!strcmp(type, "ptMetro")) return VehicleKind::Metro;
        if (!strcmp(type, "ptBusNight")) return VehicleKind::NightBus;
        if (!strncmp(type, "ptBus", 5)) return VehicleKind::Bus;
        if (!strcmp(type, "ptTrainS")) return VehicleKind::SBahn;
        if (!strncmp(type, "ptTrain", 7)) return VehicleKind::Train;
    }
    // Guess the vehicle type from the line name
    if (line && line[0]) {
        char u = (char)toupper((unsigned char)line[0]);
        size_t l = strlen(line);
        if (u == 'U' && l <= 2) return VehicleKind::Metro;
        if (u == 'N') return VehicleKind::NightBus;
        if (u == 'S' && l <= 3) return VehicleKind::SBahn;
        if (toupper((unsigned char)line[l - 1]) == 'A' || toupper((unsigned char)line[l - 1]) == 'B') return VehicleKind::Bus;
        return VehicleKind::Tram;
    }
    return VehicleKind::Other;
}

void buildWlFilter(JsonDocument &f) {
    f.clear();
    JsonObject mon = f["data"]["monitors"].add<JsonObject>();
    mon["locationStop"]["properties"]["attributes"]["rbl"] = true;
    JsonObject line = mon["lines"].add<JsonObject>();
    line["name"] = true;
    line["towards"] = true;
    line["type"] = true;
    line["realtimeSupported"] = true;
    JsonObject dep = line["departures"]["departure"].add<JsonObject>();
    dep["departureTime"]["timePlanned"] = true;
    dep["departureTime"]["timeReal"] = true;
    dep["departureTime"]["countdown"] = true;
    f["message"]["value"] = true;
    f["message"]["serverTime"] = true;
}

int parseWl(JsonDocument &doc, const MonitorConfig *cfgs, size_t n, Row *rows, time_t now) {
    // Clear departures of all WL rows, keep destination and name
    for (size_t i = 0; i < n; i++) {
        if (cfgs[i].src != Source::WL) continue;
        rows[i].nDeps = 0;
        rows[i].ok = true;
        rows[i].viaFallback = false;
        rows[i].fetchedAt = now;
    }

    int found = 0;
    for (JsonObject mon : doc["data"]["monitors"].as<JsonArray>()) {
        uint32_t rbl = mon["locationStop"]["properties"]["attributes"]["rbl"] | 0u;
        for (JsonObject line : mon["lines"].as<JsonArray>()) {
            const char *name = line["name"] | "";
            for (size_t i = 0; i < n; i++) {
                const MonitorConfig &c = cfgs[i];
                if (c.src != Source::WL || c.stop != rbl || !text::lineEquals(c.line, name)) continue;
                Row &r = rows[i];
                found++;
                text::copy(r.line, sizeof(r.line), name);
                char dest[40];
                text::copy(dest, sizeof(dest), line["towards"] | "");
                text::cleanStationName(dest);
                if (dest[0]) text::copy(r.dest, sizeof(r.dest), dest);
                r.kind = kindFromWlType(line["type"] | (const char *)nullptr, name);
                bool rtSupported = line["realtimeSupported"] | true;

                for (JsonObject d : line["departures"]["departure"].as<JsonArray>()) {
                    JsonObject t = d["departureTime"];
                    Departure dep{};
                    const char *planned = t["timePlanned"] | (const char *)nullptr;
                    const char *real = t["timeReal"] | (const char *)nullptr;
                    dep.planned = timeutil::parseIsoOffset(planned);
                    dep.real = real ? timeutil::parseIsoOffset(real) : dep.planned;
                    dep.realtime = rtSupported && real != nullptr;
                    if (!dep.planned && dep.real) dep.planned = dep.real;
                    if (!dep.planned) {
                        // only countdown available
                        int cd = t["countdown"] | -1;
                        if (cd < 0 || !timeutil::timeValid(now)) continue;
                        dep.planned = dep.real = now + cd * 60;
                    }
                    addDeparture(r, dep, now);
                }
            }
        }
    }
    return found;
}

// ÖBB

VehicleKind parseOebbProduct(const char *pr, char *lineOut, size_t n) {
    char buf[32];
    text::copy(buf, sizeof(buf), pr ? pr : "");
    char *sp = strchr(buf, ' ');
    const char *prod = buf;
    const char *rest = "";
    if (sp) { *sp = 0; rest = sp + 1; while (*rest == ' ') rest++; }

    VehicleKind k = VehicleKind::Train;
    char out[40];
    if (!strcmp(prod, "S")) {
        k = VehicleKind::SBahn;
        snprintf(out, sizeof(out), "S%s", rest);
    } else if (!strcmp(prod, "Bus") || !strcmp(prod, "AST")) {
        k = (rest[0] == 'N') ? VehicleKind::NightBus : VehicleKind::Bus;
        text::copy(out, sizeof(out), rest);
    } else if (!strcmp(prod, "Tram") || !strcmp(prod, "STR")) {
        k = VehicleKind::Tram;
        text::copy(out, sizeof(out), rest);
    } else if (!strcmp(prod, "U")) {
        k = VehicleKind::Metro;
        snprintf(out, sizeof(out), "U%s", rest);
    } else {
        // Drop train numbers (3 or more digits) for long distance and regional trains
        bool digits = rest[0] != 0;
        for (const char *p = rest; *p; p++) if (!isdigit((unsigned char)*p)) digits = false;
        if (digits && strlen(rest) >= 3) text::copy(out, sizeof(out), prod);
        else snprintf(out, sizeof(out), "%s%s", prod, rest);
    }
    text::normalizeLine(out, lineOut, n);
    return k;
}

void buildOebbFilter(JsonDocument &f) {
    f.clear();
    JsonObject j = f["journey"].add<JsonObject>();
    j["ti"] = true;
    j["da"] = true;
    j["pr"] = true;
    j["st"] = true;
    j["rt"] = true;
    f["stationName"] = true;
}

bool parseOebb(JsonDocument &doc, const MonitorConfig &cfg, Row &row, time_t now) {
    JsonArray journeys = doc["journey"].as<JsonArray>();
    if (journeys.isNull()) return false;

    row.nDeps = 0;
    row.ok = true;
    row.fetchedAt = now;
    bool destSet = false;

    for (JsonObject j : journeys) {
        char line[12];
        VehicleKind kind = parseOebbProduct(j["pr"] | "", line, sizeof(line));
        if (cfg.line[0] && !text::lineEquals(cfg.line, line)) continue;

        char dest[48];
        text::oebbToUtf8(j["st"] | "", dest, sizeof(dest));
        if (cfg.dest[0] && !text::containsFolded(dest, cfg.dest)) continue;
        text::cleanStationName(dest);

        Departure d{};
        d.planned = timeutil::parseLocalDateTime(j["da"] | "", j["ti"] | "");
        if (!d.planned) continue;
        d.real = d.planned;

        JsonVariant rt = j["rt"];
        if (rt.is<JsonObject>()) {
            d.realtime = true;
            const char *status = rt["status"] | (const char *)nullptr;
            if (status && (strstr(status, "Ausfall") || strstr(status, "ausfall"))) d.cancelled = true;
            const char *dlt = rt["dlt"] | (const char *)nullptr;
            const char *dld = rt["dld"] | (j["da"] | "");
            int dlm = atoi(rt["dlm"] | "0");
            time_t exact = dlt ? timeutil::parseLocalDateTime(dld, dlt) : 0;
            d.real = exact ? exact : d.planned + dlm * 60;
        }

        size_t before = row.nDeps;
        addDeparture(row, d, now);
        if (row.nDeps > before && !destSet) {
            // Take line and destination from the next departure
            text::copy(row.line, sizeof(row.line), line);
            text::copy(row.dest, sizeof(row.dest), dest);
            row.kind = kind;
            destSet = true;
        }
    }
    return true;
}

} // namespace transit
