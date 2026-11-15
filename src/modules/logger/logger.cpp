#include "logger.h"
#include <Adafruit_ST7735.h>

// Display defined in main.cpp
extern Adafruit_ST7735 tft;

Logger* Logger::instance = nullptr;
LogMode Logger::mode = ONLY_SERIAL;
LogLevel Logger::level = INFO;

Logger::Logger(LogMode m, LogLevel l) {
    mode = m;
    level = l;
}

void Logger::init(LogMode m, LogLevel l) {
    if (instance == nullptr) {
        instance = new Logger(m, l);
    } else {
        mode = m;
        level = l;
    }
}

Logger* Logger::getInstance() {
    if (instance == nullptr) {
        // Used when init() was not called
        instance = new Logger(ONLY_SERIAL, INFO);
    }
    return instance;
}

void Logger::log(const char *message, LogLevel msgLevel) {
    if (mode == NONE) return;

    if (msgLevel < level) return;

    String levelPrefix = "[INFO] ";
    if (msgLevel == WARNING) levelPrefix = "[WARN] ";
    if (msgLevel == ERROR)   levelPrefix = "[ERR ] ";

    String logLine = "[" + String(millis()) + "] " + levelPrefix + message;

    // Serial
    if (mode == ONLY_SERIAL || mode == SERIAL_AND_DISPLAY) {
        Serial.println(logLine);
    }

    // Display
    if (mode == ONLY_DISPLAY || mode == SERIAL_AND_DISPLAY) {
        if (msgLevel == ERROR) {
            tft.setTextColor(ST77XX_RED);
        } else if (msgLevel == WARNING) {
            tft.setTextColor(ST77XX_YELLOW);
        } else {
            tft.setTextColor(ST77XX_WHITE);
        }

        tft.println(message);
        
        tft.setTextColor(ST77XX_WHITE);

        // Clear the screen when it is full
        if (tft.getCursorY() + 7 >= tft.height()) {  // font height
            tft.fillScreen(ST77XX_BLACK);
            tft.setCursor(0, 0);
        }
    }
}

void Logger::setMode(LogMode newMode) {
    mode = newMode;
    if (newMode != ONLY_DISPLAY && newMode != SERIAL_AND_DISPLAY) {
        tft.fillScreen(ST77XX_BLACK);
        tft.setCursor(0, 0);
    }
}