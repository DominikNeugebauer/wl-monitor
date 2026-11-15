#include "transit_api.h"
#include "transit_parse.h"
#include "text_util.h"
#include "../net/http_json.h"
#include "../../config.h"

namespace transit {

static const char *WL_BASE = "https://www.wienerlinien.at/ogd_realtime/monitor";
static const char *OEBB_BOARD =
    "https://fahrplan.oebb.at/bin/stboard.exe/dn?L=vs_scotty.vs_liveticker&boardType=dep"
    "&productsFilter=1111111111111&tickerID=dep&start=yes&eqstops=false&additionalTime=0"
    "&outputMode=tickerDataOnly";
static const char *OEBB_SEARCH =
    "https://fahrplan.oebb.at/bin/ajax-getstop.exe/dn?REQ0JourneyStopsS0A=1&REQ0JourneyStopsB=12&js=true&S=";

const char *kindName(VehicleKind k) {
    switch (k) {
        case VehicleKind::Tram: return "tram";
        case VehicleKind::Bus: return "bus";
        case VehicleKind::NightBus: return "night";
        case VehicleKind::Metro: return "metro";
        case VehicleKind::SBahn: return "sbahn";
        case VehicleKind::Train: return "train";
        default: return "other";
    }
}

bool fetchWl(const MonitorConfig *cfgs, size_t n, Row *rows, time_t now, String &error) {
    String url = WL_BASE;
    char sep = '?';
    uint32_t seen[kMaxMonitors];
    size_t nSeen = 0;
    for (size_t i = 0; i < n; i++) {
        if (cfgs[i].src != Source::WL) continue;
        bool dup = false;
        for (size_t k = 0; k < nSeen; k++) if (seen[k] == cfgs[i].stop) dup = true;
        if (dup) continue;
        seen[nSeen++] = cfgs[i].stop;
        url += sep;
        url += "stopId=" + String(cfgs[i].stop);
        sep = '&';
    }
    if (nSeen == 0) return true;

    JsonDocument filter, doc;
    buildWlFilter(filter);
    if (!http::getJson(url, doc, &filter, false, error)) return false;

    const char *msg = doc["message"]["value"] | "";
    if (strcmp(msg, "OK") != 0) {
        error = String("WL: ") + msg;
        return false;
    }
    parseWl(doc, cfgs, n, rows, now);
    return true;
}

bool fetchOebb(const MonitorConfig &cfg, Row &row, time_t now, String &error) {
    String url = OEBB_BOARD;
    url += "&evaId=" + String(cfg.stop);
    url += "&dirInput=";
    if (cfg.dir) url += String(cfg.dir);
    url += "&showJourneys=" + String(OEBB_MAX_JOURNEYS);

    JsonDocument filter, doc;
    buildOebbFilter(filter);
    if (!http::getJson(url, doc, &filter, true, error)) return false;
    if (!parseOebb(doc, cfg, row, now)) {
        error = "ÖBB: unexpected response";
        return false;
    }
    return true;
}

bool wlLookup(uint32_t diva, JsonArray out, String &error) {
    String url = String(WL_BASE) + "?diva=" + String(diva);
    JsonDocument filter, doc;
    JsonObject mon = filter["data"]["monitors"].add<JsonObject>();
    mon["locationStop"]["properties"]["attributes"]["rbl"] = true;
    mon["locationStop"]["properties"]["title"] = true;
    JsonObject line = mon["lines"].add<JsonObject>();
    line["name"] = true;
    line["towards"] = true;
    line["type"] = true;
    if (!http::getJson(url, doc, &filter, false, error)) return false;

    for (JsonObject m : doc["data"]["monitors"].as<JsonArray>()) {
        uint32_t rbl = m["locationStop"]["properties"]["attributes"]["rbl"] | 0u;
        for (JsonObject l : m["lines"].as<JsonArray>()) {
            const char *name = l["name"] | "";
            bool dup = false;
            for (JsonObject e : out)
                if ((e["rbl"] | 0u) == rbl && !strcmp(e["line"] | "", name)) dup = true;
            if (dup) continue;
            JsonObject e = out.add<JsonObject>();
            e["rbl"] = rbl;
            e["line"] = name;
            e["towards"] = l["towards"] | "";
            e["kind"] = kindName(kindFromWlType(l["type"] | (const char *)nullptr, name));
            e["title"] = m["locationStop"]["properties"]["title"] | "";
        }
    }
    return true;
}

bool oebbSearch(const String &query, JsonArray out, String &error) {
    String url = String(OEBB_SEARCH) + http::urlEncode(query);
    JsonDocument filter, doc;
    JsonObject s = filter["suggestions"].add<JsonObject>();
    s["value"] = true;
    s["extId"] = true;
    s["type"] = true;
    if (!http::getJson(url, doc, &filter, true, error)) return false;

    for (JsonObject sg : doc["suggestions"].as<JsonArray>()) {
        if (strcmp(sg["type"] | "", "1") != 0) continue; // stops only, no addresses or POIs
        char name[64];
        text::oebbToUtf8(sg["value"] | "", name, sizeof(name));
        JsonObject e = out.add<JsonObject>();
        e["id"] = (uint32_t)strtoul(sg["extId"] | "0", nullptr, 10);
        e["name"] = name;
    }
    return true;
}

bool oebbLines(uint32_t eva, JsonArray out, String &error) {
    String url = String(OEBB_BOARD) + "&evaId=" + String(eva) + "&dirInput=&showJourneys=40";
    JsonDocument filter, doc;
    buildOebbFilter(filter);
    if (!http::getJson(url, doc, &filter, true, error)) return false;

    for (JsonObject j : doc["journey"].as<JsonArray>()) {
        char line[12];
        VehicleKind k = parseOebbProduct(j["pr"] | "", line, sizeof(line));
        char dest[48];
        text::oebbToUtf8(j["st"] | "", dest, sizeof(dest));
        text::cleanStationName(dest);
        bool dup = false;
        for (JsonObject e : out)
            if (!strcmp(e["line"] | "", line) && !strcmp(e["dest"] | "", dest)) dup = true;
        if (dup) continue;
        JsonObject e = out.add<JsonObject>();
        e["line"] = line;
        e["dest"] = dest;
        e["kind"] = kindName(k);
    }
    return true;
}

// Fallback over ÖBB

static bool fetchBoard(uint32_t eva, int journeys, JsonDocument &doc, String &error) {
    String url = String(OEBB_BOARD) + "&evaId=" + String(eva) + "&dirInput=&showJourneys=" + String(journeys);
    JsonDocument filter;
    buildOebbFilter(filter);
    return http::getJson(url, doc, &filter, true, error);
}

static bool boardHasLine(JsonDocument &doc, const char *line) {
    for (JsonObject j : doc["journey"].as<JsonArray>()) {
        char l[12];
        parseOebbProduct(j["pr"] | "", l, sizeof(l));
        if (text::lineEquals(l, line)) return true;
    }
    return false;
}

bool oebbFindStopForLine(const char *wlStopName, const char *line, uint32_t &eva, String &error) {
    char name[40];
    text::copy(name, sizeof(name), wlStopName);
    text::cleanStationName(name); // "Krottenbachstraße S" becomes "Krottenbachstraße"
    if (!name[0]) { error = "no stop name"; return false; }

    // ÖBB names Vienna stops like "Wien Billrothstraße/Gatterburggasse", so search for "Wien <name>"
    String url = String(OEBB_SEARCH) + http::urlEncode(String("Wien ") + name);
    JsonDocument filter, doc;
    JsonObject f = filter["suggestions"].add<JsonObject>();
    f["value"] = true;
    f["extId"] = true;
    f["type"] = true;
    if (!http::getJson(url, doc, &filter, true, error)) return false;

    uint32_t cand[4];
    int nCand = 0;
    // First take names that contain the WL name, then all other stops
    for (int round = 0; round < 2 && nCand < 4; round++) {
        for (JsonObject sg : doc["suggestions"].as<JsonArray>()) {
            if (nCand >= 4) break;
            if (strcmp(sg["type"] | "", "1") != 0) continue;
            char v[64];
            text::oebbToUtf8(sg["value"] | "", v, sizeof(v));
            bool match = text::containsFolded(v, name);
            if ((round == 0) != match) continue;
            cand[nCand++] = (uint32_t)strtoul(sg["extId"] | "0", nullptr, 10);
        }
        if (round == 0 && nCand > 0) break;
    }
    if (nCand == 0) { error = String("no \xC3\x96" "BB stop found for ") + name; return false; }

    // Use the first station where the line actually stops
    for (int i = 0; i < nCand; i++) {
        JsonDocument board;
        String err;
        if (!fetchBoard(cand[i], 40, board, err)) { error = err; continue; }
        if (boardHasLine(board, line)) {
            eva = cand[i];
            return true;
        }
    }
    if (!error.length()) error = String("line ") + line + " not found at " + name;
    return false;
}

bool fetchOebbFallback(const MonitorConfig &wlCfg, uint32_t eva, const char *destKey, Row &row,
                       time_t now, String &error) {
    MonitorConfig tmp{};
    tmp.src = Source::OEBB;
    tmp.stop = eva;
    text::copy(tmp.line, sizeof(tmp.line), wlCfg.line);
    text::copy(tmp.dest, sizeof(tmp.dest), destKey ? destKey : "");

    // WL stops often have many lines, so load more departures
    JsonDocument doc;
    if (!fetchBoard(eva, 40, doc, error)) return false;
    if (!parseOebb(doc, tmp, row, now)) {
        error = "\xC3\x96" "BB: unexpected response";
        return false;
    }
    row.viaFallback = true;
    return true;
}

} // namespace transit
