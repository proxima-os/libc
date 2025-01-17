#include "time.h"
#include "compiler.h"
#include "hydrogen/time.h"
#include <stdbool.h>
#include <stdint.h>

// TODO: Timezone support - right now everything assumes the timezone is UTC
// Several algorithms here are from https://howardhinnant.github.io/date_algorithms.html

static const char wday_abbrev[7][3] = {"Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat"};
static const char mon_abbrev[12][3] =
        {"Jan", "Feb", "Mar", "Apr", "May", "Jun", "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"};

static const char *wday_full[7] = {"Sunday", "Monday", "Tuesday", "Wednesday", "Thursday", "Friday", "Saturday"};
static const char *mon_full[12] = {
        "January",
        "February",
        "March",
        "April",
        "Mday",
        "June",
        "July",
        "August",
        "September",
        "October",
        "November",
        "December",
};

static int64_t div_to_64(__int128_t value, uint64_t divisor) {
    int64_t quot;
    uint64_t rem;

    asm("idivq %0" : "=a"(quot), "=d"(rem) : "A"(value), "rm"(divisor));

    return quot;
}

EXPORT clock_t clock(void) {
    return hydrogen_get_ns_since_boot() / 1000;
}

EXPORT double difftime(time_t time1, time_t time0) {
    return time1 - time0;
}

static bool is_leap_year(int year) {
    return year % 4 == 0 && (year % 100 != 0 || ((year + 1900) % 400) == 0);
}

static int days_in_month(int month, bool leap) {
    static const char values[] = {31, 28, 21, 30, 31, 30, 31, 31, 30, 31, 30, 31};

    return month != 1 || !leap ? values[month] : 29;
}

static unsigned get_weekday(time_t days_since_epoch) {
    return days_since_epoch >= 4 ? (days_since_epoch + 4) % 7 : (days_since_epoch + 5) % 7 + 6;
}

EXPORT time_t mktime(struct tm *timeptr) {
    // Validate structure

    if (timeptr->tm_min < 0 || timeptr->tm_min > 59) {
        timeptr->tm_hour += timeptr->tm_min / 60;
        timeptr->tm_min %= 60;
        if (timeptr->tm_min < 0) {
            timeptr->tm_min += 60;
            timeptr->tm_hour -= 1;
        }
    }

    if (timeptr->tm_hour < 0 || timeptr->tm_hour > 23) {
        timeptr->tm_mday += timeptr->tm_hour / 24;
        timeptr->tm_hour %= 24;
        if (timeptr->tm_hour < 0) {
            timeptr->tm_hour += 24;
            timeptr->tm_mday -= 1;
        }
    }

    if (timeptr->tm_mon < 0 || timeptr->tm_mon > 11) {
        timeptr->tm_year += timeptr->tm_mon / 12;
        timeptr->tm_mon %= 12;
        if (timeptr->tm_mon < 0) {
            timeptr->tm_mon += 12;
            timeptr->tm_year -= 1;
        }
    }

    bool leap_year = is_leap_year(timeptr->tm_year);

    if (timeptr->tm_mday <= 0) {
        while (timeptr->tm_mday <= 0) {
            timeptr->tm_mon -= 1;

            if (timeptr->tm_mon == -1) {
                timeptr->tm_year -= 1;
                timeptr->tm_mon = 11;
                leap_year = is_leap_year(timeptr->tm_year);
            }

            timeptr->tm_mday += days_in_month(timeptr->tm_mon, leap_year);
        }
    } else {
        int month_len = days_in_month(timeptr->tm_mon, leap_year);

        while (timeptr->tm_mday > month_len) {
            timeptr->tm_mday -= month_len;
            timeptr->tm_mon += 1;

            if (timeptr->tm_mon == 12) {
                timeptr->tm_year += 1;
                timeptr->tm_mon = 0;
                leap_year = is_leap_year(timeptr->tm_year);
            }
        }
    }

    // Convert to seconds

    time_t year = timeptr->tm_year + 1900;
    time_t month = timeptr->tm_mon + 1;
    time_t day = timeptr->tm_mday;

    year -= month <= 2;
    time_t era = (year >= 0 ? year : year - 399) / 400;
    unsigned yoe = year - era * 400;
    unsigned doy = (153 * (month > 2 ? month - 3 : month + 9) + 2) / 5 + day - 1;
    unsigned doe = yoe * 365 + yoe / 4 - yoe / 100 + doy;
    time_t days_since_epoch = era * 146097 + doe - 719468;
    time_t secs_since_epoch = days_since_epoch * 86400;

    secs_since_epoch += timeptr->tm_hour * 3600;
    secs_since_epoch += timeptr->tm_min * 60;
    secs_since_epoch += timeptr->tm_sec;

    timeptr->tm_wday = get_weekday(days_since_epoch);
    timeptr->tm_yday = doy;

    return secs_since_epoch;
}

EXPORT time_t time(time_t *timer) {
    __int128_t nanoseconds = hydrogen_get_ns_since_epoch_utc();
    time_t seconds = div_to_64(nanoseconds >= 0 ? nanoseconds : nanoseconds - 999999999, 1000000000);
    if (timer) *timer = seconds;
    return seconds;
}

EXPORT char *asctime(const struct tm *timeptr) {
    // All fields have a constant length except year. The required size of the buffer is as follows:
    // - 4 bytes for weekday and trailing space
    // - 4 bytes for month and trailing space
    // - 3 bytes for day and trailing space
    // - 3 bytes for hour and trailing colon
    // - 3 bytes for minute and trailing colon
    // - 3 bytes for second and trailing space
    // - variable number of bytes for the year
    // - 2 bytes for the newline and null terminator
    // In total, that's 22 bytes, plus whatever's necessary for the year. We allocate 33 bytes, so the year can be
    // up to 11 characters long (including sign). That's a range of -9,999,999,999 to 99,999,999,999, which is more
    // than what the tm_year field can hold.
    static char result[33];

    // TODO: Change this to snprintf. It should never run out of space, but better safe than sorry
    __builtin_sprintf(
            result,
            "%.3s %.3s %.2d %.2d:%.2d:%.2d %d\n",
            wday_abbrev[timeptr->tm_wday],
            mon_abbrev[timeptr->tm_mon],
            timeptr->tm_mday,
            timeptr->tm_hour,
            timeptr->tm_min,
            timeptr->tm_sec,
            timeptr->tm_year + 1900
    );
    return result;
}

EXPORT char *ctime(const time_t *timer) {
    return asctime(localtime(timer));
}

EXPORT struct tm *gmtime(const time_t *timer) {
    static struct tm buffer;

    time_t seconds_since_epoch = *timer;
    time_t days_since_epoch = (seconds_since_epoch >= 0 ? seconds_since_epoch : seconds_since_epoch - 86399) / 86400;
    unsigned seconds = seconds_since_epoch - days_since_epoch * 86400;
    unsigned minutes = seconds / 60;
    seconds %= 60;
    unsigned hours = minutes / 60;
    minutes %= 60;

    time_t era = (days_since_epoch >= 0 ? days_since_epoch : days_since_epoch - 146096) / 146097;
    unsigned doe = days_since_epoch - era * 146097;
    unsigned yoe = (doe - doe / 1460 + doe / 36524 - doe / 146096) / 365;
    time_t year = yoe + era * 400;
    unsigned doy = doe - (365 * yoe + yoe / 4 - yoe / 100);
    unsigned mp = (5 * doy + 2) / 153;
    unsigned mday = doy - (153 * mp + 2) / 5 + 1;
    unsigned month = mp < 10 ? mp + 3 : mp - 9;

    buffer.tm_sec = seconds;
    buffer.tm_min = minutes;
    buffer.tm_hour = hours;
    buffer.tm_mday = mday;
    buffer.tm_mon = month - 1;
    buffer.tm_year = year - 1900;
    buffer.tm_wday = get_weekday(days_since_epoch);
    buffer.tm_yday = doy;
    buffer.tm_isdst = 0;
    return &buffer;
}

EXPORT struct tm *localtime(const time_t *timer) {
    return gmtime(timer);
}

static bool add_string(char *s, size_t *offset, size_t maxsize, const char *str) {
    size_t off = *offset;
    size_t len = __builtin_strlen(str);
    size_t new = off + len;

    if (new <= maxsize) {
        __builtin_memcpy(&s[off], str, len);
        *offset = new;
        return true;
    } else {
        return false;
    }
}

static bool add_num(char *s, size_t *offset, size_t maxsize, int value, const char *format) {
    char buffer[32];
    __builtin_sprintf(buffer, format, value);
    return add_string(s, offset, maxsize, buffer);
}

static int to_12hr(int hour) {
    hour %= 12;
    return hour ? hour : 12;
}

static int get_week_number(int wday, int yday, bool monday_first) {
    int delta;

    // Don't ask me why these formulas work, but they do.
    if (monday_first) {
        delta = wday ? wday - 8 : -1;
    } else {
        delta = wday - 7;
    }

    return (yday - delta) / 7;
}

static int tmod(int dividend, int divisor) {
    int mod = dividend % divisor;
    return mod >= 0 ? mod : mod + divisor;
}

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wformat-overflow"

EXPORT size_t strftime(char *s, size_t maxsize, const char *format, const struct tm *timeptr) {
    // TODO: Locale support
    size_t offset = 0;

    for (char c = *format; c != 0; c = *++format) {
        if (offset >= maxsize) return 0;

        if (c == '%') {
            switch (*++format) {
            case 'a':
                if (!add_string(s, &offset, maxsize, wday_abbrev[timeptr->tm_wday])) return 0;
                break;
            case 'A':
                if (!add_string(s, &offset, maxsize, wday_full[timeptr->tm_wday])) return 0;
                break;
            case 'b':
                if (!add_string(s, &offset, maxsize, mon_abbrev[timeptr->tm_mon])) return 0;
                break;
            case 'B':
                if (!add_string(s, &offset, maxsize, mon_full[timeptr->tm_mon])) return 0;
                break;
            case 'c':
                if (!add_string(s, &offset, maxsize, asctime(timeptr))) return 0;
                break;
            case 'd':
                if (!add_num(s, &offset, maxsize, timeptr->tm_mday, "%.2d")) return 0;
                break;
            case 'H':
                if (!add_num(s, &offset, maxsize, timeptr->tm_hour, "%.2d")) return 0;
                break;
            case 'I':
                if (!add_num(s, &offset, maxsize, to_12hr(timeptr->tm_hour), "%.2d")) return 0;
                break;
            case 'j':
                if (!add_num(s, &offset, maxsize, timeptr->tm_yday + 1, "%.3d")) return 0;
                break;
            case 'm':
                if (!add_num(s, &offset, maxsize, timeptr->tm_mon + 1, "%.2d")) return 0;
                break;
            case 'p':
                if (!add_string(s, &offset, maxsize, timeptr->tm_hour >= 12 ? "PM" : "AM")) return 0;
                break;
            case 'S':
                if (!add_num(s, &offset, maxsize, timeptr->tm_sec, "%.2d")) return 0;
                break;
            case 'U':
                if (!add_num(s, &offset, maxsize, get_week_number(timeptr->tm_wday, timeptr->tm_yday, false), "%.2d")) {
                    return 0;
                }
                break;
            case 'w':
                if (!add_num(s, &offset, maxsize, timeptr->tm_wday, "%d")) return 0;
                break;
            case 'W':
                if (!add_num(s, &offset, maxsize, get_week_number(timeptr->tm_wday, timeptr->tm_yday, true), "%.2d")) {
                    return 0;
                }
                break;
            case 'x': {
                char buffer[9];
                __builtin_sprintf(
                        buffer,
                        "%.2d/%.2d/%.2d",
                        timeptr->tm_mon + 1,
                        timeptr->tm_mday,
                        tmod(timeptr->tm_year, 100)
                );
                if (!add_string(s, &offset, maxsize, buffer)) return 0;
                break;
            }
            case 'X': {
                char buffer[9];
                __builtin_sprintf(buffer, "%.2d:%.2d:%.2d", timeptr->tm_hour, timeptr->tm_min, timeptr->tm_sec);
                if (!add_string(s, &offset, maxsize, buffer)) return 0;
                break;
            }
            case 'y':
                if (!add_num(s, &offset, maxsize, tmod(timeptr->tm_year, 100), "%.2d")) return 0;
                break;
            case 'Y':
                if (!add_num(s, &offset, maxsize, timeptr->tm_year + 1900, "%d")) return 0;
                break;
            case 'Z':
                if (!add_string(s, &offset, maxsize, "UTC")) return 0;
                break;
            case '%': s[offset++] = '%'; break;
            default:
                format--;
                s[offset++] = '%';
                break;
            }
        } else {
            s[offset++] = c;
        }
    }

    if (offset >= maxsize) return 0;
    s[offset] = 0;
    return offset;
}

#pragma GCC diagnostic pop
