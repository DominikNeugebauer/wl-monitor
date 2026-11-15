#ifndef LOGGER_H
#define LOGGER_H

#include <Arduino.h>

enum LogLevel {
    INFO,
    WARNING,
    ERROR
};

enum LogMode {
    ONLY_SERIAL,
    ONLY_DISPLAY,
    SERIAL_AND_DISPLAY,
    NONE
};

class Logger {
public:
    static void init(LogMode mode, LogLevel level);
    static Logger *getInstance();
    void log(const char *message, LogLevel msgLevel = INFO);
    void setMode(LogMode newMode);
private:
    Logger(LogMode mode, LogLevel level);
    static LogMode mode;
    static LogLevel level;
    static Logger *instance;
};

#endif