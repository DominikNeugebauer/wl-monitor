#include "time_util.h"
#include <stdio.h>
#include <string.h>

namespace timeutil {

// Howard Hinnant: days_from_civil
static long daysFromCivil(int y, unsigned m, unsigned d) {
    y -= m <= 2;
    const long era = (y >= 0 ? y : y - 399) / 400;
    const unsigned yoe = (unsigned)(y - era * 400);
    const unsigned doy = (153 * (m + (m > 2 ? -3 : 9)) + 2) / 5 + d - 1;
    const unsigned doe = yoe * 365 + yoe / 4 - yoe / 100 + doy;
    return era * 146097 + (long)doe - 719468;
}

time_t parseIsoOffset(const char *s) {
    if (!s) return 0;
    int Y, M, D, h, m, sec;
    if (sscanf(s, "%4d-%2d-%2dT%2d:%2d:%2d", &Y, &M, &D, &h, &m, &sec) != 6) return 0;
    long offsetSec = 0;
    const char *tz = s + 19;
    if (*tz == '.') { tz++; while (*tz >= '0' && *tz <= '9') tz++; }
    if (*tz == '+' || *tz == '-') {
        int sign = (*tz == '-') ? -1 : 1;
        int oh = 0, om = 0;
        if (sscanf(tz + 1, "%2d:%2d", &oh, &om) == 2 || sscanf(tz + 1, "%2d%2d", &oh, &om) >= 1)
            offsetSec = sign * (oh * 3600L + om * 60L);
    }
    long days = daysFromCivil(Y, (unsigned)M, (unsigned)D);
    long long t = (long long)days * 86400LL + h * 3600LL + m * 60LL + sec - offsetSec;
    return (time_t)t;
}

time_t parseLocalDateTime(const char *date, const char *hhmm) {
    if (!date || !hhmm) return 0;
    int d, mo, y, h, mi;
    if (sscanf(date, "%d.%d.%d", &d, &mo, &y) != 3) return 0;
    if (sscanf(hhmm, "%d:%d", &h, &mi) != 2) return 0;
    struct tm tmv;
    memset(&tmv, 0, sizeof(tmv));
    tmv.tm_year = y - 1900;
    tmv.tm_mon = mo - 1;
    tmv.tm_mday = d;
    tmv.tm_hour = h;
    tmv.tm_min = mi;
    tmv.tm_isdst = -1;
    return mktime(&tmv);
}

bool timeValid(time_t now) {
    return now > 1700000000; // after Nov 2023
}

} // namespace timeutil
