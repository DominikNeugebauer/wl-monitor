// WL-Monitor
// Departure board for Wiener Linien and ÖBB on an ESP32-S3 with an ST7735 display.
//
// On first start the device opens the setup network "WL-Monitor-XXXX" (password: fahrplan).
// Connect to it and pick your home WiFi in the captive portal.
// After that the web interface is at http://wl-monitor.local or the IP shown on the display.
// Departures are loaded every minute. Hold the BOOT button for 5 s to clear the WiFi credentials.
#include <Arduino.h>
#include <SPI.h>
#include <Adafruit_GFX.h>
#include <Adafruit_ST7735.h>

#include "config.h"
#include "modules/logger/logger.h"
#include "modules/settings/settings.h"
#include "modules/net/wifi_manager.h"
#include "modules/web/web_portal.h"
#include "modules/transit/transit_service.h"
#include "modules/transit/time_util.h"
#include "modules/display/ui.h"

#if TFT_USE_HW_SPI
// Hardware SPI
Adafruit_ST7735 tft = Adafruit_ST7735(&SPI, TFT_CS, TFT_DC, TFT_RST);
#else
// Software SPI
Adafruit_ST7735 tft = Adafruit_ST7735(TFT_CS, TFT_DC, TFT_MOSI, TFT_SCLK, TFT_RST);
#endif

static AppSettings    appSettings;
static WifiManager    wifi;
static WebPortal      portal;
static TransitService transitService;
static Ui            *ui = nullptr;
static Row            rows[kMaxMonitors];

static uint32_t bootMs = 0;
static uint32_t lastFrameMs = 0;
static uint32_t frame = 0;
static uint32_t showConnectedUntil = 0;
static bool     wasConnected = false;
static uint32_t btnDownSince = 0;

// Display
static void setBacklight(uint8_t level) {
#if ESP_ARDUINO_VERSION_MAJOR >= 3
    ledcWrite(TFT_BL, level);
#else
    ledcWrite(0, level);
#endif
}

static void pushFrame();
static bool forceFullFrameFlag();

static void applyDisplaySettings() {
    tft.setRotation(appSettings.flip ? 3 : 1);
    setBacklight(appSettings.brightness);
    forceFullFrameFlag(); // redraw everything after rotation
}

static void initDisplay() {
#if ESP_ARDUINO_VERSION_MAJOR >= 3
    ledcAttach(TFT_BL, 5000, 8);
#else
    ledcSetup(0, 5000, 8);
    ledcAttachPin(TFT_BL, 0);
#endif
    setBacklight(200);

#if TFT_USE_HW_SPI
    SPI.begin(TFT_SCLK, -1, TFT_MOSI, TFT_CS);
#endif
    tft.initR(TFT_INIT_TAB);
    tft.setRotation(1);
    tft.fillScreen(ST77XX_BLUE);
}

// Only bands of 8 rows that changed are sent to the display.
// A full frame over software SPI takes about 0.5 s and would block the web server.
static const int16_t kBand = 8;
static uint32_t bandHash[Ui::H / kBand];
static bool forceFullFrame = true;

static uint32_t hashPixels(const uint16_t *p, size_t n) {
    uint32_t h = 2166136261u; // FNV-1a
    for (size_t i = 0; i < n; i++) { h ^= p[i]; h *= 16777619u; }
    return h;
}

static void pushFrame() {
    uint16_t *buf = ui->buffer();
    for (int b = 0; b < Ui::H / kBand; b++) {
        uint16_t *p = buf + b * kBand * Ui::W;
        uint32_t h = hashPixels(p, (size_t)kBand * Ui::W);
        if (!forceFullFrame && h == bandHash[b]) continue;
        bandHash[b] = h;
        tft.drawRGBBitmap(0, b * kBand, p, Ui::W, kBand);
    }
    forceFullFrame = false;
}

static bool forceFullFrameFlag() {
    forceFullFrame = true;
    return true;
}

// BOOT button: short press shows the IP address, hold 5 s to forget WiFi
static void pollButton() {
    bool down = digitalRead(BTN_PIN) == LOW;
    if (down && !btnDownSince) btnDownSince = millis();
    if (!down && btnDownSince) {
        uint32_t held = millis() - btnDownSince;
        btnDownSince = 0;
        if (held >= 30 && held < 1000) showConnectedUntil = millis() + 8000;
    }
}

static bool handleButton() {
    if (!btnDownSince) return false;
    uint32_t held = millis() - btnDownSince;
    if (held < 1000) return false;

    if (held >= 5000) {
        ui->drawMessage("WLAN gel\xC3\xB6scht", "Neustart...", nullptr, 0xF800);
        pushFrame();
        settings::clearWifi();
        delay(1200);
        ESP.restart();
    }
    char buf[32];
    snprintf(buf, sizeof(buf), "noch %lu s halten", (unsigned long)((5000 - held) / 1000 + 1));
    ui->drawMessage("WLAN l\xC3\xB6schen?", buf, "loslassen = abbrechen", 0xFD20);
    pushFrame();
    return true;
}

void setup() {
    Serial.begin(115200);
    delay(1500); // give the USB serial monitor time to attach
    bootMs = millis();

    // Log to serial only, the display is used by the UI
    Logger::init(LogMode::ONLY_SERIAL, INFO);
    Logger::getInstance()->log("WL-Monitor " FW_VERSION " starting");

    pinMode(LED_PIN, OUTPUT);
    digitalWrite(LED_PIN, HIGH);
    pinMode(BTN_PIN, INPUT_PULLUP);

    settings::load(appSettings);
    initDisplay();
    ui = new Ui();
    if (!ui->buffer()) Logger::getInstance()->log("Framebuffer allocation failed", ERROR);
    applyDisplaySettings();

    // Boot animation
    for (uint32_t f = 0; f < 16; f++) {
        ui->drawSplash(f);
        pushFrame();
        delay(45);
    }

    // Vienna time zone, synced over NTP once WiFi is up
    configTzTime(timeutil::kTzVienna, NTP_SERVER_1, NTP_SERVER_2);

    wifi.onProvisioned([](const String &ssid, const String &pass) {
        settings::saveWifi(ssid.c_str(), pass.c_str());
        strlcpy(appSettings.wifiSsid, ssid.c_str(), sizeof(appSettings.wifiSsid));
        strlcpy(appSettings.wifiPass, pass.c_str(), sizeof(appSettings.wifiPass));
        Logger::getInstance()->log("WiFi credentials saved");
    });
    wifi.begin(appSettings.wifiSsid, appSettings.wifiPass);

    transitService.begin();
    transitService.setConfig(appSettings.monitors, appSettings.nMonitors);

    portal.begin(wifi, appSettings, transitService, applyDisplaySettings);
}

void loop() {
    wifi.loop();
    portal.loop();
    pollButton();

    const uint32_t now = millis();
    const bool connected = wifi.staConnected();
    if (connected && !wasConnected) showConnectedUntil = now + 5000; // show the IP for a few seconds
    wasConnected = connected;

    // Departure screen at 2 fps, animations at 5 fps
    const uint32_t interval = (connected && now > showConnectedUntil) ? 500 : 200;
    if (now - lastFrameMs < interval) {
        delay(2);
        return;
    }
    lastFrameMs = now;
    frame++;

    if (handleButton()) return;

    if (connected && now < showConnectedUntil) {
        String ip = WiFi.localIP().toString();
        ui->drawConnected(ip.c_str(), APP_HOSTNAME ".local");
    } else if (connected) {
        size_t n = transitService.snapshot(rows, kMaxMonitors);
        time_t t = time(nullptr);
        UiStatus st;
        st.wifiOk = true;
        st.rssi = WiFi.RSSI();
        st.timeOk = timeutil::timeValid(t);
        st.fetchError = transitService.lastError();
        st.fetching = transitService.fetching();
        st.refreshProgress = transitService.progress();
        st.tick = (now - bootMs) / 1000;
        ui->drawDepartures(rows, n, t, st);
    } else if (wifi.state() == StaState::Connecting && (!wifi.apActive() || wifi.apClients() > 0)) {
        ui->drawConnecting(wifi.pendingSsid().c_str(), frame);
    } else if (wifi.apActive()) {
        String ip = wifi.apIp().toString();
        ui->drawSetup(wifi.apSsid().c_str(), AP_PASSWORD, ip.c_str(),
                      wifi.hasCredentials() ? wifi.ssid().c_str() : "", frame);
    } else {
        ui->drawConnecting(wifi.ssid().c_str(), frame);
    }
    pushFrame();
}
