#include "ui.h"
#include "../transit/text_util.h"
#include "../transit/time_util.h"
#include <Fonts/FreeSansBold9pt7b.h>
#include <string.h>
#include <stdio.h>
#include <math.h>

// Colors (RGB565)
static constexpr uint16_t rgb(uint8_t r, uint8_t g, uint8_t b) {
    return (uint16_t)(((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3));
}

namespace col {
static constexpr uint16_t bg        = rgb(8, 11, 24);
static constexpr uint16_t rowA      = rgb(11, 16, 34);
static constexpr uint16_t rowB      = rgb(16, 23, 48);
static constexpr uint16_t headTop   = rgb(34, 42, 96);
static constexpr uint16_t headBot   = rgb(16, 20, 50);
static constexpr uint16_t white     = rgb(242, 245, 255);
static constexpr uint16_t text2     = rgb(138, 150, 184);
static constexpr uint16_t text3     = rgb(80, 90, 125);
static constexpr uint16_t accent    = rgb(62, 199, 255);
static constexpr uint16_t accentDim = rgb(27, 36, 72);
static constexpr uint16_t green     = rgb(52, 224, 122);
static constexpr uint16_t yellow    = rgb(255, 210, 63);
static constexpr uint16_t orange    = rgb(255, 150, 28);
static constexpr uint16_t red       = rgb(255, 77, 94);
static constexpr uint16_t plan      = rgb(169, 193, 255);
static constexpr uint16_t purple    = rgb(167, 110, 255);

// Line colors
static constexpr uint16_t tram      = rgb(227, 0, 15);
static constexpr uint16_t bus       = rgb(36, 89, 201);
static constexpr uint16_t nightBg   = rgb(27, 31, 75);
static constexpr uint16_t nightFg   = rgb(255, 212, 0);
static constexpr uint16_t sbahn     = rgb(0, 148, 216);
static constexpr uint16_t train     = rgb(226, 0, 42);
static constexpr uint16_t u1        = rgb(227, 0, 15);
static constexpr uint16_t u2        = rgb(167, 98, 163);
static constexpr uint16_t u3        = rgb(238, 125, 0);
static constexpr uint16_t u4        = rgb(0, 149, 64);
static constexpr uint16_t u5        = rgb(0, 160, 176);
static constexpr uint16_t u6        = rgb(158, 106, 52);
} // namespace col

static uint16_t lerp565(uint16_t a, uint16_t b, float t) {
    int ar = (a >> 11) & 0x1F, ag = (a >> 5) & 0x3F, ab = a & 0x1F;
    int br = (b >> 11) & 0x1F, bg = (b >> 5) & 0x3F, bb = b & 0x1F;
    int r = ar + (int)((br - ar) * t), g = ag + (int)((bg - ag) * t), bl = ab + (int)((bb - ab) * t);
    return (uint16_t)((r << 11) | (g << 5) | bl);
}

static const GFXfont *kBold = &FreeSansBold9pt7b;

Ui::Ui() : c(W, H) {
    c.cp437(true);
    c.setTextWrap(false);
}

// Helpers

// y is the top edge for the default font and the baseline for GFX fonts
void Ui::text(int16_t x, int16_t y, const char *utf8, uint16_t color, const GFXfont *font) {
    char buf[64];
    text::utf8ToCp437(utf8, buf, sizeof(buf));
    c.setFont(font);
    c.setTextSize(1);
    c.setTextColor(color);
    c.setCursor(x, y);
    c.print(buf);
    c.setFont(nullptr);
}

int16_t Ui::textWidth(const char *cp437, const GFXfont *font) {
    if (!font) return (int16_t)(strlen(cp437) * 6);
    c.setFont(font);
    int16_t x1, y1;
    uint16_t w, h;
    c.getTextBounds(cp437, 0, 0, &x1, &y1, &w, &h);
    c.setFont(nullptr);
    return (int16_t)(w + x1);
}

void Ui::centerText(int16_t y, const char *utf8, uint16_t color, const GFXfont *font) {
    char buf[64];
    text::utf8ToCp437(utf8, buf, sizeof(buf));
    int16_t w = textWidth(buf, font);
    c.setFont(font);
    c.setTextColor(color);
    c.setCursor((W - w) / 2, y);
    c.print(buf);
    c.setFont(nullptr);
}

void Ui::gradientRect(int16_t x, int16_t y, int16_t w, int16_t h, uint16_t top, uint16_t bottom) {
    for (int16_t i = 0; i < h; i++) c.drawFastHLine(x, y + i, w, lerp565(top, bottom, h > 1 ? (float)i / (h - 1) : 0));
}

void Ui::wifiIcon(int16_t x, int16_t y, int bars, uint16_t on, uint16_t off) {
    // 4 bars, 11x9 px, (x, y) is the top left corner
    for (int i = 0; i < 4; i++) {
        int16_t h = 3 + i * 2;
        c.fillRect(x + i * 3, y + 9 - h, 2, h, i < bars ? on : off);
    }
}

void Ui::spinner(int16_t cx, int16_t cy, int16_t r, uint32_t frame, uint16_t color) {
    for (int i = 0; i < 8; i++) {
        float a = (float)i * (float)M_PI / 4.0f;
        int16_t x = cx + (int16_t)lroundf(cosf(a) * r);
        int16_t y = cy + (int16_t)lroundf(sinf(a) * r);
        int age = (int)((frame - i + 800) % 8); // 0 is the head
        float t = 1.0f - age / 8.0f;
        c.fillCircle(x, y, age == 0 ? 3 : 2, lerp565(col::bg, color, t));
    }
}

static uint16_t badgeColor(const Row &r, uint16_t &fg) {
    fg = col::white;
    switch (r.kind) {
        case VehicleKind::Tram: return col::tram;
        case VehicleKind::Bus: return col::bus;
        case VehicleKind::NightBus: fg = col::nightFg; return col::nightBg;
        case VehicleKind::SBahn: return col::sbahn;
        case VehicleKind::Train: return col::train;
        case VehicleKind::Metro:
            switch (r.line[1]) {
                case '1': return col::u1;
                case '2': return col::u2;
                case '3': return col::u3;
                case '4': return col::u4;
                case '5': return col::u5;
                case '6': return col::u6;
                default: return col::u1;
            }
        default: return col::text3;
    }
}

static int delayMinutes(const Departure &d) {
    long diff = (long)(d.real - d.planned);
    return (int)((diff >= 0 ? diff + 30 : diff - 30) / 60);
}

static uint16_t depColor(const Departure &d) {
    if (d.cancelled) return col::red;
    if (!d.realtime) return col::plan;
    int dl = delayMinutes(d);
    if (dl <= 0) return col::green;
    if (dl <= 2) return col::yellow;
    if (dl <= 5) return col::orange;
    return col::red;
}

// Screens

void Ui::drawSplash(uint32_t frame) {
    c.fillScreen(col::bg);
    // Logo
    c.fillRoundRect(52, 22, 56, 40, 8, col::tram);
    int16_t w = textWidth("WL", kBold);
    c.setFont(kBold);
    c.setTextColor(col::white);
    c.setCursor(80 - w / 2, 48);
    c.print("WL");
    c.setFont(nullptr);
    centerText(72, "Abfahrtsmonitor", col::white);
    centerText(84, "Wiener Linien + \xC3\x96" "BB", col::text2);
    // Line color stripes
    const uint16_t stripes[] = {col::u1, col::u2, col::u3, col::u4, col::u6, col::sbahn, col::bus, col::tram};
    int16_t sw = W / 8;
    for (int i = 0; i < 8; i++) {
        int16_t h = (int16_t)((frame > (uint32_t)i) ? ((frame - i) * 2 > 8 ? 8 : (frame - i) * 2) : 0);
        c.fillRect(i * sw, H - h, sw, h, stripes[i]);
    }
}

void Ui::drawConnecting(const char *ssid, uint32_t frame) {
    c.fillScreen(col::bg);
    spinner(80, 44, 16, frame, col::accent);
    centerText(80, "Verbinde mit WLAN", col::text2);
    char s[40];
    text::copy(s, sizeof(s), ssid);
    centerText(94, s, col::white);
    uint8_t dots = frame % 4;
    char d[5] = "   ";
    for (uint8_t i = 0; i < dots; i++) d[i] = '.';
    centerText(108, d, col::accent);
}

void Ui::drawConnected(const char *ip, const char *host) {
    c.fillScreen(col::bg);
    c.fillCircle(80, 38, 18, col::green);
    // Check mark
    for (int o = -1; o <= 1; o++) {
        c.drawLine(71, 38 + o, 78, 45 + o, col::bg);
        c.drawLine(78, 45 + o, 90, 31 + o, col::bg);
    }
    centerText(76, "Verbunden!", col::white, kBold);
    centerText(88, ip, col::accent);
    char buf[48];
    snprintf(buf, sizeof(buf), "http://%s", host);
    centerText(102, buf, col::text2);
}

void Ui::drawSetup(const char *apSsid, const char *apPass, const char *apIp,
                   const char *retrySsid, uint32_t frame) {
    c.fillScreen(col::bg);
    gradientRect(0, 0, W, 20, rgb(88, 52, 170), rgb(40, 24, 90));
    text(6, 15, "Einrichtung", col::white, kBold);
    // Pulsing WiFi icon
    int bars = (int)(frame % 5);
    wifiIcon(143, 5, bars, col::white, rgb(70, 50, 130));

    text(6, 26, "1. Handy verbinden mit:", col::text2);
    c.fillRoundRect(4, 36, W - 8, 26, 4, col::rowB);
    c.drawRoundRect(4, 36, W - 8, 26, 4, col::purple);
    text(9, 40, apSsid, col::accent);
    char pw[40];
    snprintf(pw, sizeof(pw), "Passwort: %s", apPass);
    text(9, 51, pw, col::white);

    text(6, 68, "2. Seite \xC3\xB6" "ffnet sich", col::text2);
    text(6, 78, "   automatisch, sonst:", col::text2);
    char url[32];
    snprintf(url, sizeof(url), "http://%s", apIp);
    text(18, 90, url, col::yellow);

    if (retrySsid && retrySsid[0]) {
        char r[48];
        snprintf(r, sizeof(r), "Suche weiter: %s", retrySsid);
        char cp[48];
        text::utf8ToCp437(r, cp, sizeof(cp));
        text::fitCp437(cp, 25);
        c.setTextColor(col::text3);
        c.setCursor(6, 114);
        c.print(cp);
    }
}

void Ui::drawMessage(const char *title, const char *line1, const char *line2, uint16_t accent) {
    c.fillScreen(col::bg);
    c.fillRect(0, 0, W, 4, accent);
    centerText(50, title, col::white, kBold);
    if (line1) centerText(66, line1, col::text2);
    if (line2) centerText(80, line2, col::text2);
}

// Departure board

void Ui::header(time_t now, const UiStatus &st, int page, int pages) {
    gradientRect(0, 0, W, 16, col::headTop, col::headBot);

    if (st.timeOk) {
        struct tm lt;
        localtime_r(&now, &lt);
        char hh[4], mm[4];
        snprintf(hh, sizeof(hh), "%02d", lt.tm_hour);
        snprintf(mm, sizeof(mm), "%02d", lt.tm_min);
        // Clock with blinking colon
        c.setFont(kBold);
        c.setTextColor(col::white);
        c.setCursor(3, 13);
        c.print(hh);
        int16_t x = c.getCursorX();
        c.setTextColor((now % 2) ? col::white : col::text3);
        c.print(":");
        c.setCursor(x + 5, 13);
        c.setTextColor(col::white);
        c.print(mm);
        int16_t xEnd = c.getCursorX();
        c.setFont(nullptr);

        static const char *wd[] = {"So", "Mo", "Di", "Mi", "Do", "Fr", "Sa"};
        char date[24];
        snprintf(date, sizeof(date), "%s %02d.%02d.", wd[lt.tm_wday], lt.tm_mday, lt.tm_mon + 1);
        text(xEnd + 5, 5, date, col::text2);
    } else {
        text(4, 5, "Zeit wird geladen...", col::text2);
    }

    int16_t x = W - 14;
    wifiIcon(x, 3, st.wifiOk ? (st.rssi > -55 ? 4 : st.rssi > -65 ? 3 : st.rssi > -75 ? 2 : 1) : 0,
             st.wifiOk ? col::accent : col::red, col::text3);
    x -= 6;
    if (st.fetchError) {
        c.fillCircle(x, 7, 4, col::red);
        c.drawFastVLine(x, 5, 3, col::white);
        c.drawPixel(x, 9, col::white);
        x -= 10;
    }
    if (pages > 1) {
        for (int i = pages - 1; i >= 0; i--) {
            c.fillCircle(x, 8, 2, i == page ? col::white : col::text3);
            x -= 6;
        }
    }

    // Progress until the next fetch
    c.drawFastHLine(0, 16, W, col::accentDim);
    if (st.fetching) {
        int16_t p = (int16_t)((st.tick * 23) % W);
        c.drawFastHLine(p, 16, 30, col::accent);
    } else {
        float p = st.refreshProgress < 0 ? 0 : st.refreshProgress > 1 ? 1 : st.refreshProgress;
        c.drawFastHLine(0, 16, (int16_t)(W * p), col::accent);
    }
}

void Ui::badge(int16_t x, int16_t y, int16_t w, int16_t h, const Row &r) {
    uint16_t fg;
    uint16_t bgc = badgeColor(r, fg);
    c.fillRoundRect(x, y, w, h, 4, bgc);
    if (r.kind == VehicleKind::NightBus) c.drawRoundRect(x, y, w, h, 4, col::nightFg);
    char l[8];
    text::copy(l, sizeof(l), r.line);
    size_t len = strlen(l);
    if (len > 5) l[5] = 0, len = 5;
    int16_t tw = (int16_t)(len * 6 - 1);
    c.setTextColor(fg);
    c.setCursor(x + (w - tw) / 2 + 1, y + (h - 7) / 2);
    c.print(l);
}

void Ui::row(int16_t y, const Row &r, time_t now, const UiStatus &st, bool alt) {
    const int16_t h = 22;
    c.fillRect(0, y, W, h, alt ? col::rowB : col::rowA);
    badge(3, y + 3, 30, 16, r);
    if (r.viaFallback) {
        // Dot on the badge when the data comes from ÖBB instead of Wiener Linien
        c.fillCircle(32, y + 4, 3, alt ? col::rowB : col::rowA);
        c.fillCircle(32, y + 4, 2, col::plan);
    }

    // Skip departures that are already gone
    const Departure *deps[kMaxDepsPerRow];
    int nd = 0;
    for (int i = 0; i < r.nDeps; i++) {
        if (st.timeOk && r.deps[i].real < now - 30) continue;
        deps[nd++] = &r.deps[i];
    }

    // Countdown
    int16_t cdLeft = W - 4;
    if (nd > 0 && st.timeOk) {
        const Departure &d = *deps[0];
        long secs = (long)(d.real - now);
        long mins = secs <= 0 ? 0 : secs / 60;
        uint16_t cc = depColor(d);
        if (d.cancelled) {
            int16_t w = textWidth("X", kBold);
            c.setFont(kBold);
            cdLeft = W - 4 - w;
            c.setTextColor(col::red);
            c.setCursor(cdLeft, y + 17);
            c.print("X");
            c.setFont(nullptr);
        } else if (mins <= 0) {
            // Departing now
            int16_t cx = W - 12, cy = y + 11;
            c.fillCircle(cx, cy, (st.tick % 2) ? 5 : 3, cc);
            c.drawCircle(cx, cy, 7, lerp565(alt ? col::rowB : col::rowA, cc, 0.5f));
            cdLeft = cx - 9;
        } else if (mins >= 100) {
            struct tm lt;
            time_t t = d.real;
            localtime_r(&t, &lt);
            char buf[8];
            snprintf(buf, sizeof(buf), "%02d:%02d", lt.tm_hour, lt.tm_min);
            cdLeft = W - 3 - 29;
            c.setTextColor(cc);
            c.setCursor(cdLeft, y + 8);
            c.print(buf);
        } else {
            char buf[8];
            snprintf(buf, sizeof(buf), "%ld", mins);
            int16_t w = textWidth(buf, kBold);
            cdLeft = W - 8 - w;
            c.setFont(kBold);
            c.setTextColor(cc);
            c.setCursor(cdLeft, y + 17);
            c.print(buf);
            c.setFont(nullptr);
            // Minute mark
            c.fillRect(W - 6, y + 5, 2, 4, cc);
        }
    }

    // Destination
    const int16_t tx = 37;
    char dest[40];
    text::utf8ToCp437(r.dest, dest, sizeof(dest));
    int16_t maxChars = (int16_t)((cdLeft - 3 - tx) / 6);
    if (maxChars < 3) maxChars = 3;
    text::fitCp437(dest, (size_t)maxChars);
    c.setTextColor(col::white);
    c.setCursor(tx, y + 3);
    c.print(dest);

    // Second line: delay and next departures
    int16_t sx = tx, sy = y + 13;
    if (nd == 0) {
        text(sx, sy, r.ok ? "keine Abfahrten" : (r.fetchedAt ? "veraltet" : "lade..."),
             r.ok ? col::text3 : (r.fetchedAt ? col::orange : col::text3));
        return;
    }
    if (!st.timeOk) return;

    const Departure &d0 = *deps[0];
    char buf[16];
    if (d0.cancelled) {
        text(sx, sy, "f\xC3\xA4llt aus", col::red);
        sx += 10 * 6;
    } else if (d0.realtime) {
        int dl = delayMinutes(d0);
        if (dl > 0) {
            snprintf(buf, sizeof(buf), "+%d", dl);
            text(sx, sy, buf, depColor(d0));
            sx += (int16_t)(strlen(buf) * 6 + 4);
        } else {
            // On time
            c.fillCircle(sx + 2, sy + 3, 2, col::green);
            sx += 8;
        }
    } else {
        text(sx, sy, "Plan", col::plan);
        sx += 4 * 6 + 4;
    }

    for (int i = 1; i < nd; i++) {
        long m = (long)(deps[i]->real - now) / 60;
        if (m < 0) m = 0;
        if (m >= 100) break;
        snprintf(buf, sizeof(buf), "%ld'", m);
        int16_t w = (int16_t)(strlen(buf) * 6);
        if (sx + w > cdLeft - 2) break;
        uint16_t cc = deps[i]->cancelled ? col::red : col::text2;
        text(sx, sy, buf, cc);
        sx += w + 4;
    }
}

void Ui::drawDepartures(const Row *rows, size_t n, time_t now, const UiStatus &st) {
    c.fillScreen(col::bg);
    const int perPage = 5;
    int pages = n == 0 ? 1 : (int)((n + perPage - 1) / perPage);
    int page = pages > 1 ? (int)((st.tick / 8) % pages) : 0;
    header(now, st, page, pages);

    if (n == 0) {
        centerText(58, "Keine Linien gew\xC3\xA4hlt", col::white);
        centerText(74, "Web-Oberfl\xC3\xA4" "che \xC3\xB6" "ffnen:", col::text2);
        centerText(88, "http://wl-monitor.local", col::accent);
        return;
    }
    int16_t y = 17;
    for (int i = page * perPage; i < (int)n && i < (page + 1) * perPage; i++) {
        row(y, rows[i], now, st, (i % 2) == 1);
        y += 22;
    }
}
