// matth-x/MicroOcpp
// Copyright Matthias Akstaller 2019 - 2025
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

#define MO_JSONDATE_SIZE (JSONDATE_LENGTH + 1)
#define MO_INTERNALTIME_SIZE sizeof("i65535t-2147483648m999") //longest possible string: max bootNr, max negative time and milliseconds

#define MO_MIN_TIME (((2010 - 1970) * 365 + 10) * 24 * 3600) // unix time for 2010-01-01Z00:00:00T (= 40 years + 10 leap days, in seconds)
#define MO_MAX_TIME (((2037 - 1970) * 365 + 17) * 24 * 3600) // unix time for 2037-01-01Z00:00:00T (= 67 years + 17 leap days, in seconds)

namespace MicroOcpp {

class Context;
class Clock;

class Timestamp : public MemoryManaged {
private:
    int32_t time = 0; //Unix time (number of seconds since Jan 1, 1970 UTC, not counting leap seconds)
    uint16_t bootNr = 0; //bootNr when timestamp was taken

#if MO_ENABLE_TIMESTAMP_MILLISECONDS
    int16_t ms = 0; //fractional ms of timestamp. Compound timestamp = time + ms. Range should be 0...999
#endif //MO_ENABLE_TIMESTAMP_MILLISECONDS

public:

    Timestamp();
    Timestamp(const Timestamp& other);
    MicroOcpp::Timestamp& operator=(const Timestamp& other) = default;

    bool isUnixTime() const;
    bool isUptime() const;
    bool isDefined() const;

friend class Clock;
};

class Clock {
private:
    Context& context;

    Timestamp unixTime;
    Timestamp uptime;

    uint32_t lastIncrement = 0;

public:

    Clock(Context& context);

    bool setup();

    void loop();

    const Timestamp &now();

    const Timestamp &getUptime();
    int32_t getUptimeInt();

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
     * Same as `setTime(const char* jsonDateString)`, but expects Unix time as seconds since 1970-01-01
     */
    bool setTime(int32_t unixTimeInt);

    /*
     * Time delta between two timestamps. Calculates t2 - t1, writes the result in seconds into dt and
     * returns true, if successful and false otherwise. The delta between two timestamps is defined if
     * t1 and t2 have a unix time, or if the clock can determine their unix time after having connected
     * to the OCPP server, or if both have a processor uptime.
     */
    bool delta(const Timestamp& t2, const Timestamp& t1, int32_t& dt) const;

    /*
     * Add secs seconds to `t`. After this operation, t will be later timestamp if secs is positive, or
     * t will be an earlier timestamp if secs is negative. This operation is only valid if t remains in
     * its numerical limits. I.e. if t is a processor uptime timestamp, then secs must not move it outside
     * the uptime range and vice versa for unix timestamps. Returns true if successful, or false if
     * operation is invalid or would overflow
     */
    bool add(Timestamp& t, int32_t secs) const;

#if MO_ENABLE_TIMESTAMP_MILLISECONDS
    bool deltaMs(const Timestamp& t2, const Timestamp& t1, int32_t& dtMs) const;
    bool addMs(Timestamp& t, int32_t ms) const;
#endif //MO_ENABLE_TIMESTAMP_MILLISECONDS

    /*
     * Print the ISO8601 represenation of t into dst, having `size` bytes. Fails if the unix time is
     * not defined and cannot be determined by the clock. `size` must be at least MO_JSONDATE_SIZE
     */
    bool toJsonString(const Timestamp& t, char *dst, size_t size) const;

    /*
     * Print an internal representation of t into dst, having `size` bytes. `size` must be at least
     * MO_INTERNALTIME_SIZE
     */
    bool toInternalString(const Timestamp& t, char *dst, size_t size) const;

    /*
     * Parses the time either from the ISO8601 representation, or the internal representation. Returns
     * true on success and false on failure.
     */
    bool parseString(const char *src, Timestamp& dst) const;

    /* Converts src into a unix time and writes result to dst. dst will equal to src if src is already
     * a unix time. Returns true if successful and false if the unix time cannot be determined */
    bool toUnixTime(const Timestamp& src, Timestamp& dst) const;
    bool toUnixTime(const Timestamp& src, int32_t& dstInt) const;

    bool fromUnixTime(Timestamp& dst, int32_t unixTimeInt) const;

    uint16_t getBootNr() const;
};

} //namespace MicroOcpp

#endif
