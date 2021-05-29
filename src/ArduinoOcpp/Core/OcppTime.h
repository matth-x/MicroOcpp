// matth-x/ArduinoOcpp
// Copyright Matthias Akstaller 2019 - 2021
// MIT License

#ifndef OCPPTIME_H
#define OCPPTIME_H

#include <functional>

#include <Arduino.h> 

#define OCPP_JSONDATE_LENGTH 24

namespace ArduinoOcpp {

typedef int32_t otime_t; //requires 32bit signed integer or bigger
typedef std::function<otime_t()> OcppClock;

namespace Clocks {

/*
 * Basic clock implementation. Works if millis() is exact enough for you and if device doesn't go in sleep mode. 
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

    bool toJsonString(char *out, size_t buffsize);

    OcppTimestamp operator+(int secs);

    OcppTimestamp operator-(int secs);

};

class OcppTime {
private:

    OcppTimestamp ocpp_basetime;
    otime_t system_basetime = 0; //your system clock's time at the moment when the OCPP server's time was taken

    OcppClock system_clock = [] () {return (otime_t) 0;};

public:

    OcppTime(OcppClock system_clock);

    otime_t getOcppTimeScalar(); //returns current time of the OCPP server in non-UNIX but signed integer format. t2 - t1 is the time difference in seconds. 
    OcppTimestamp createTimestamp(otime_t scalar); //creates a timestamp in a JSON-serializable format. createTimestamp(getOcppTimeScalar()) will return the current OCPP time

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


};

}

#endif
