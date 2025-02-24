// matth-x/MicroOcpp
// Copyright Matthias Akstaller 2019 - 2024
// MIT License

#ifndef MO_TIME_H
#define MO_TIME_H

#include <functional>
#include <stdint.h>
#include <stddef.h>

#include <MicroOcpp/Core/Memory.h>
#include <MicroOcpp/Platform.h>

#ifndef MO_ENABLE_TIMESTAMP_MILLISECONDS
#define MO_ENABLE_TIMESTAMP_MILLISECONDS 0
#endif

#if MO_ENABLE_TIMESTAMP_MILLISECONDS
#define JSONDATE_LENGTH 24   //max. ISO 8601 date length, excluding the terminating zero
#else
#define JSONDATE_LENGTH 20   //ISO 8601 date length, excluding the terminating zero
#endif //MO_ENABLE_TIMESTAMP_MILLISECONDS

namespace MicroOcpp {

class Timestamp : public MemoryManaged {
private:
    int32_t time; //Unix time (number of seconds since Jan 1, 1970 UTC, not counting leap seconds)

#if MO_ENABLE_TIMESTAMP_MILLISECONDS
    int16_t ms = 0; //fractional ms of timestamp. Compound timestamp = time + ms. Range should be [0, 999]
#endif //MO_ENABLE_TIMESTAMP_MILLISECONDS

public:

    Timestamp();

    Timestamp(const Timestamp& other);

#if MO_ENABLE_TIMESTAMP_MILLISECONDS
    Timestamp(int16_t year, int16_t month, int16_t day, int32_t hour, int32_t minute, int32_t second, int32_t ms = 0);
#else 
    Timestamp(int16_t year, int16_t month, int16_t day, int32_t hour, int32_t minute, int32_t second);
#endif //MO_ENABLE_TIMESTAMP_MILLISECONDS

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

#if MO_ENABLE_TIMESTAMP_MILLISECONDS
    Timestamp &addMilliseconds(int ms);
#endif //MO_ENABLE_TIMESTAMP_MILLISECONDS

    /*
     * Time periods are given in seconds for all of the following arithmetic operations
     */
    Timestamp &operator+=(int secs);
    Timestamp &operator-=(int secs);

    int operator-(const Timestamp &rhs) const;

    operator int32_t() const;

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
