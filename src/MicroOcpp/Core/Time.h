// matth-x/MicroOcpp
// Copyright Matthias Akstaller 2019 - 2023
// MIT License

#ifndef MO_TIME_H
#define MO_TIME_H

#include <functional>
#include <stdint.h>
#include <stddef.h>

#include <MicroOcpp/Platform.h>

#define JSONDATE_LENGTH 24

namespace MicroOcpp {

class Timestamp {
private:
    /*
     * Internal representation of the current time. The initial values correspond to UNIX-time 0. January
     * corresponds to month 0 and the first day in the month is day 0.
     */
    int16_t year = 1970;
    int16_t month = 0;
    int16_t day = 0;
    int32_t hour = 0;
    int32_t minute = 0;
    int32_t second = 0;
    int32_t ms = 0;

public:

    Timestamp();

    Timestamp(const Timestamp& other);

    Timestamp(int16_t year, int16_t month, int16_t day, int32_t hour, int32_t minute, int32_t second, int32_t ms = 0) :
                year(year), month(month), day(day), hour(hour), minute(minute), second(second), ms(ms) { };

    /**
     * Expects a date string like
     * 2020-10-01T20:53:32.486Z
     * 
     * as generated in JavaScript by calling toJSON() on a Date object
     * 
     * Only processes the first 19 characters. The subsequent are ignored until terminating 0.
     * 
     * Has a semi-sophisticated type check included. Will return true on successful time set and false if
     * the given string is not a JSON Date string.
     * 
     * jsonDateString: 0-terminated string
     */
    bool setTime(const char* jsonDateString);

    bool toJsonString(char *out, size_t buffsize) const;

    Timestamp &operator=(const Timestamp &rhs);

    Timestamp &addMilliseconds(int ms);

    /*
     * Time periods are given in seconds for all of the following arithmetic operations
     */
    Timestamp &operator+=(int secs);
    Timestamp &operator-=(int secs);

    int operator-(const Timestamp &rhs) const;

    friend Timestamp operator+(const Timestamp &lhs, int secs);
    friend Timestamp operator-(const Timestamp &lhs, int secs);

    friend bool operator==(const Timestamp &lhs, const Timestamp &rhs);
    friend bool operator!=(const Timestamp &lhs, const Timestamp &rhs);
    friend bool operator<(const Timestamp &lhs, const Timestamp &rhs);
    friend bool operator<=(const Timestamp &lhs, const Timestamp &rhs);
    friend bool operator>(const Timestamp &lhs, const Timestamp &rhs);
    friend bool operator>=(const Timestamp &lhs, const Timestamp &rhs);
};

extern const Timestamp MIN_TIME;
extern const Timestamp MAX_TIME;

class Clock {
private:

    Timestamp mocpp_basetime = Timestamp();
    decltype(mocpp_tick_ms()) system_basetime = 0; //the value of mocpp_tick_ms() when OCPP server's time was taken
    decltype(mocpp_tick_ms()) lastUpdate = 0;

    Timestamp currentTime = Timestamp();

public:

    Clock();
    Clock(const Clock&) = delete;
    Clock(const Clock&&) = delete;
    Clock& operator=(const Clock&) = delete;

    const Timestamp &now();

    /**
     * Expects a date string like
     * 2020-10-01T20:53:32.486Z
     * 
     * as generated in JavaScript by calling toJSON() on a Date object
     * 
     * Only processes the first 23 characters. The subsequent are ignored
     * 
     * Has a semi-sophisticated type check included. Will return true on successful time set and false if
     * the given string is not a JSON Date string.
     * 
     * jsonDateString: 0-terminated string
     */
    bool setTime(const char* jsonDateString);

    /*
     * Timestamps which were taken before the Clock was initially set can be adjusted retrospectively. Two
     * conditions must be true: the Clock was set in the meantime and the Timestamp was taken at the same
     * run of this library. The caller must check this
     */
    Timestamp adjustPrebootTimestamp(const Timestamp& t);
};

}

#endif
