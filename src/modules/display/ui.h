#ifndef UI_H
#define UI_H

#include <Adafruit_GFX.h>
#include "../transit/transit_types.h"

// Draws all screens into a 16-bit framebuffer (GFXcanvas16, 160x128).
// main.cpp pushes the finished frame to the display.
// This class does not depend on the TFT or WiFi.

struct UiStatus {
    bool     wifiOk;
    int      rssi;            // dBm
    bool     timeOk;
    bool     fetchError;      // last fetch failed, fully or partly
    bool     fetching;        // fetch in progress
    float    refreshProgress; // 0..1 until the next fetch
    uint32_t tick;            // seconds since boot, used for animations and paging
};

class Ui {
public:
    static const int16_t W = 160;
    static const int16_t H = 128;

    Ui();
    GFXcanvas16 &canvas() { return c; }
    uint16_t *buffer() { return c.getBuffer(); }

    void drawSplash(uint32_t frame);
    void drawConnecting(const char *ssid, uint32_t frame);
    void drawConnected(const char *ip, const char *host);
    void drawSetup(const char *apSsid, const char *apPass, const char *apIp,
                   const char *retrySsid, uint32_t frame);
    void drawDepartures(const Row *rows, size_t n, time_t now, const UiStatus &st);
    void drawMessage(const char *title, const char *line1, const char *line2, uint16_t accent);

private:
    GFXcanvas16 c;

    void text(int16_t x, int16_t y, const char *utf8, uint16_t color, const GFXfont *font = nullptr);
    int16_t textWidth(const char *cp437, const GFXfont *font = nullptr);
    void centerText(int16_t y, const char *utf8, uint16_t color, const GFXfont *font = nullptr);
    void gradientRect(int16_t x, int16_t y, int16_t w, int16_t h, uint16_t top, uint16_t bottom);
    void wifiIcon(int16_t x, int16_t y, int bars, uint16_t on, uint16_t off);
    void spinner(int16_t cx, int16_t cy, int16_t r, uint32_t frame, uint16_t color);

    void header(time_t now, const UiStatus &st, int page, int pages);
    void badge(int16_t x, int16_t y, int16_t w, int16_t h, const Row &row);
    void row(int16_t y, const Row &row, time_t now, const UiStatus &st, bool alt);
};

#endif
