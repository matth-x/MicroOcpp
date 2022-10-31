// matth-x/ArduinoOcpp
// Copyright Matthias Akstaller 2019 - 2022
// MIT License

#ifndef OCPPTIME_H
#define OCPPTIME_H

#include <functional>
#include <stdint.h>
#include <stddef.h>

namespace ArduinoOcpp {

typedef int32_t otime_t; //requires 32bit signed integer or bigger
#define OTIME_MAX ((otime_t) INT32_MAX)
typedef std::function<otime_t()> OcppClock;

#define INFINITY_THLD (OTIME_MAX - ((otime_t) (400 * 24 * 3600))) //Upper limiter for valid time range. From this value on, a scalar time means "infinity". It's 400 days before the "year 2038" problem.
#define JSONDATE_LENGTH 24

namespace Clocks {

/*
 * Basic clock implementation. Works if ao_tick_ms() is exact enough for you and if device doesn't go in sleep mode. 
 */
extern OcppClock DEFAULT_CLOCK;
} //end namespace Clocks

class OcppTimestamp {
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

public:

    OcppTimestamp();

    OcppTimestamp(int16_t year, int16_t month, int16_t day, int32_t hour, int32_t minute, int32_t second) :
                year(year), month(month), day(day), hour(hour), minute(minute), second(second) { };

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

    OcppTimestamp &operator=(const OcppTimestamp &rhs);

    OcppTimestamp &operator+=(int secs);
    OcppTimestamp &operator-=(int secs);

    otime_t operator-(const OcppTimestamp &rhs) const;

    friend OcppTimestamp operator+(const OcppTimestamp &lhs, int secs);
    friend OcppTimestamp operator-(const OcppTimestamp &lhs, int secs);

    friend bool operator==(const OcppTimestamp &lhs, const OcppTimestamp &rhs);
    friend bool operator!=(const OcppTimestamp &lhs, const OcppTimestamp &rhs);
    friend bool operator<(const OcppTimestamp &lhs, const OcppTimestamp &rhs);
    friend bool operator<=(const OcppTimestamp &lhs, const OcppTimestamp &rhs);
    friend bool operator>(const OcppTimestamp &lhs, const OcppTimestamp &rhs);
    friend bool operator>=(const OcppTimestamp &lhs, const OcppTimestamp &rhs);
};

extern const OcppTimestamp MIN_TIME;
extern const OcppTimestamp MAX_TIME;

class OcppTime {
private:

    OcppTimestamp ocpp_basetime = OcppTimestamp();
    otime_t system_basetime = 0; //your system clock's time at the moment when the OCPP server's time was taken
    bool ocppTimeIsSet = false;

    OcppClock system_clock = [] () {return (otime_t) 0;};

    OcppTimestamp currentTime = OcppTimestamp();
    otime_t previousUpdate = -1;

public:

    OcppTime(const OcppClock& system_clock);
    //OcppTime(const OcppTime& ocppTime) = default;

    otime_t getOcppTimeScalar(); //returns current time of the OCPP server in non-UNIX but signed integer format. t2 - t1 is the time difference in seconds. 
    const OcppTimestamp &getOcppTimestampNow();
    OcppTimestamp createTimestamp(otime_t scalar); //creates a timestamp in a JSON-serializable format. createTimestamp(getOcppTimeScalar()) will return the current OCPP time
    otime_t toOcppTimeScalar(const OcppTimestamp &otimestamp);

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
    bool setOcppTime(const char* jsonDateString);

    bool isValid() {return ocppTimeIsSet;}
};

}

#endif
