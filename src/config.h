#ifndef APP_CONFIG_H
#define APP_CONFIG_H

// Hardware: ESP32-S3 N16R8 with 1.8" ST7735 display (160x128)
#define TFT_MOSI  8   // IO8
#define TFT_SCLK  9   // IO9
#define TFT_CS    10  // IO10
#define TFT_DC    11  // IO11 (DS/RS)
#define TFT_RST   12  // IO12
#define TFT_BL    13  // IO13 (backlight, PWM)
#define LED_PIN   35  // IO35 (status LED)
#define BTN_PIN   0   // BOOT button, hold 5 s to clear WiFi credentials

// Try INITR_GREENTAB or INITR_REDTAB if colors or offsets are wrong
#define TFT_INIT_TAB INITR_BLACKTAB

// 0: software SPI (known to work)
// 1: hardware SPI (faster, not fully tested)
#define TFT_USE_HW_SPI 0

// Network
#define APP_HOSTNAME        "wl-monitor"        // http://wl-monitor.local
#define AP_SSID_PREFIX      "WL-Monitor-"       // followed by 4 hex digits of the MAC
#define AP_PASSWORD         "fahrplan"          // at least 8 chars, empty for an open network
#define STA_CONNECT_TIMEOUT_MS  20000UL         // timeout per connection attempt
#define STA_RETRY_INTERVAL_MS   30000UL         // retry interval while WiFi is down
#define AP_FALLBACK_AFTER_MS    90000UL         // start the setup AP again after this long without WiFi
#define AP_LINGER_MS           120000UL         // keep the AP open for a while after setup

#define NTP_SERVER_1 "pool.ntp.org"
#define NTP_SERVER_2 "time.cloudflare.com"

// Departures
#define FETCH_INTERVAL_MS 60000UL   // refresh every minute
#define OEBB_MAX_JOURNEYS 12        // max trains per ÖBB request
// If the Wiener Linien API is not reachable, WL rows are loaded from ÖBB instead.
// WL is then retried every 5 min.
#define WL_RETRY_INTERVAL_MS      300000UL
#define FALLBACK_RESOLVE_RETRY_MS 600000UL  // retry after 10 min if no ÖBB station was found

#define FW_VERSION "1.0.0"

#endif
