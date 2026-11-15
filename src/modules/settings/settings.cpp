#include "settings.h"
#include "../transit/text_util.h"
#include <Preferences.h>

namespace settings {

static const char *NS = "wlmon";

static MonitorConfig make(Source src, uint32_t stop, uint32_t dir, const char *line, const char *dest, const char *name) {
    MonitorConfig c{};
    c.src = src;
    c.stop = stop;
    c.dir = dir;
    text::copy(c.line, sizeof(c.line), line);
    text::copy(c.dest, sizeof(c.dest), dest);
    text::copy(c.stopName, sizeof(c.stopName), name);
    return c;
}

void loadDefaultMonitors(AppSettings &s) {
    // RBL numbers and evaIds taken from the live APIs (Sept 2026)
    s.nMonitors = 0;
    // S45 Krottenbachstraße towards Handelskai (via Oberdöbling 1291904)
    s.monitors[s.nMonitors++] = make(Source::OEBB, 1291902, 1291904, "S45", "", "Krottenbachstraße");
    // S45 Krottenbachstraße towards Hütteldorf (via Gersthof 1291801)
    s.monitors[s.nMonitors++] = make(Source::OEBB, 1291902, 1291801, "S45", "", "Krottenbachstraße");
    // 38 Gatterburggasse to Schottentor
    s.monitors[s.nMonitors++] = make(Source::WL, 2985, 0, "38", "Schottentor", "Gatterburggasse");
    // 35A Obersteinergasse to Spittelau
    s.monitors[s.nMonitors++] = make(Source::WL, 1262, 0, "35A", "Spittelau", "Obersteinergasse");
    // 10A Chimanistraße to Niederhofstraße
    s.monitors[s.nMonitors++] = make(Source::WL, 2965, 0, "10A", "Niederhofstraße", "Chimanistraße");
}

void monitorsToJson(const AppSettings &s, JsonArray arr) {
    for (uint8_t i = 0; i < s.nMonitors; i++) {
        const MonitorConfig &c = s.monitors[i];
        JsonObject o = arr.add<JsonObject>();
        o["src"] = c.src == Source::WL ? "wl" : "oebb";
        o["stop"] = c.stop;
        if (c.dir) o["dir"] = c.dir;
        o["line"] = c.line;
        if (c.dest[0]) o["dest"] = c.dest;
        o["name"] = c.stopName;
    }
}

bool monitorsFromJson(JsonArrayConst arr, AppSettings &s, String *error) {
    AppSettings tmp = s;
    tmp.nMonitors = 0;
    for (JsonObjectConst o : arr) {
        if (tmp.nMonitors >= kMaxMonitors) {
            if (error) *error = "Maximal " + String(kMaxMonitors) + " Einträge";
            return false;
        }
        const char *src = o["src"] | "";
        MonitorConfig c{};
        if (!strcmp(src, "wl")) c.src = Source::WL;
        else if (!strcmp(src, "oebb")) c.src = Source::OEBB;
        else {
            if (error) *error = "Unbekannte Quelle";
            return false;
        }
        c.stop = o["stop"] | 0u;
        c.dir = o["dir"] | 0u;
        text::copy(c.line, sizeof(c.line), o["line"] | "");
        text::copy(c.dest, sizeof(c.dest), o["dest"] | "");
        text::copy(c.stopName, sizeof(c.stopName), o["name"] | "");
        if (!c.stop || (c.src == Source::WL && !c.line[0])) {
            if (error) *error = "Eintrag unvollständig";
            return false;
        }
        tmp.monitors[tmp.nMonitors++] = c;
    }
    s = tmp;
    return true;
}

void load(AppSettings &s) {
    memset(&s, 0, sizeof(s));
    s.brightness = 200;

    Preferences p;
    p.begin(NS, true);
    text::copy(s.wifiSsid, sizeof(s.wifiSsid), p.getString("ssid", "").c_str());
    text::copy(s.wifiPass, sizeof(s.wifiPass), p.getString("pass", "").c_str());
    s.brightness = p.getUChar("bright", 200);
    s.flip = p.getBool("flip", false);
    String mon = p.getString("mon", "");
    p.end();

    bool ok = false;
    if (mon.length()) {
        JsonDocument doc;
        if (!deserializeJson(doc, mon)) ok = monitorsFromJson(doc.as<JsonArrayConst>(), s, nullptr);
    }
    if (!ok) {
        loadDefaultMonitors(s);
        return;
    }

    // Older saved WL entries have no destination, which the ÖBB fallback needs.
    // Fill it in from the defaults for the default lines.
    AppSettings def;
    loadDefaultMonitors(def);
    for (uint8_t i = 0; i < s.nMonitors; i++) {
        MonitorConfig &c = s.monitors[i];
        if (c.src != Source::WL || c.dest[0]) continue;
        for (uint8_t k = 0; k < def.nMonitors; k++) {
            const MonitorConfig &d = def.monitors[k];
            if (d.src == Source::WL && d.stop == c.stop && text::lineEquals(d.line, c.line))
                text::copy(c.dest, sizeof(c.dest), d.dest);
        }
    }
}

void saveWifi(const char *ssid, const char *pass) {
    Preferences p;
    p.begin(NS, false);
    p.putString("ssid", ssid);
    p.putString("pass", pass);
    p.end();
}

void clearWifi() {
    Preferences p;
    p.begin(NS, false);
    p.remove("ssid");
    p.remove("pass");
    p.end();
}

void saveMonitors(const AppSettings &s) {
    JsonDocument doc;
    monitorsToJson(s, doc.to<JsonArray>());
    String out;
    serializeJson(doc, out);
    Preferences p;
    p.begin(NS, false);
    p.putString("mon", out);
    p.end();
}

void saveDisplay(const AppSettings &s) {
    Preferences p;
    p.begin(NS, false);
    p.putUChar("bright", s.brightness);
    p.putBool("flip", s.flip);
    p.end();
}

} // namespace settings
