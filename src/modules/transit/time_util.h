#ifndef TIME_UTIL_H
#define TIME_UTIL_H

#include <time.h>

namespace timeutil {

// POSIX time zone for Vienna (CET/CEST)
static const char *const kTzVienna = "CET-1CEST,M3.5.0,M10.5.0/3";

// Parses "2026-09-24T21:19:40.000+0200" to UTC epoch, 0 on error
time_t parseIsoOffset(const char *s);

// Parses "24.09.2026" and "21:27" in Vienna local time to UTC epoch, 0 on error.
// TZ must be set before.
time_t parseLocalDateTime(const char *date, const char *hhmm);

// True once the system time is synced
bool timeValid(time_t now);

} // namespace timeutil

#endif
