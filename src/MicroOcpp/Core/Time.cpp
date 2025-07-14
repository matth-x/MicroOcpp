// matth-x/MicroOcpp
// Copyright Matthias Akstaller 2019 - 2025
// MIT License

#include <MicroOcpp/Core/Time.h>

#include <string.h>
#include <ctype.h>

#include <MicroOcpp/Context.h>
#include <MicroOcpp/Core/PersistencyUtils.h>
#include <MicroOcpp/Debug.h>

#define MO_MAX_UPTIME (MO_MIN_TIME - 1)

// It's a common mistake to roll-over the mocpp_tick_ms callback too early which breaks
// the unsigned integer arithmetics in this library. The ms counter should use the full
// value range up to (32^2 - 1) ms before resetting to 0. Clock checks for this issue
// and skips one time increment if it is detected. This can be disabled by defining
// MO_CLOCK_ROLLOVER_PROTECTION=0 in the build system
#ifndef MO_CLOCK_ROLLOVER_PROTECTION
#define MO_CLOCK_ROLLOVER_PROTECTION 1
#endif

namespace MicroOcpp {
int noDays(int month, int year) {
    return (month == 0 || month == 2 || month == 4 || month == 6 || month == 7 || month == 9 || month == 11) ? 31 :
            ((month == 3 || month == 5 || month == 8 || month == 10) ? 30 :
            ((year % 4 == 0 && (year % 100 != 0 || year % 400 == 0)) ? 29 : 28));
}
} //namespace MicroOcpp

using namespace MicroOcpp;

Timestamp::Timestamp() : MemoryManaged("Timestamp") {

}

Timestamp::Timestamp(const Timestamp& other) {
    *this = other;
    updateMemoryTag(other.getMemoryTag(), nullptr);
}

bool Timestamp::isUnixTime() const {
    return time >= MO_MIN_TIME && time <= MO_MAX_TIME;
}

bool Timestamp::isUptime() const {
    return time <= MO_MAX_UPTIME;
}

bool Timestamp::isDefined() const {
    return bootNr != 0;
}

Clock::Clock(Context& context) : context(context) {

}

bool Clock::setup() {

    uint16_t bootNr = 1; //0 is reserved for undefined

    if (auto filesystem = context.getFilesystem()) {
        // restore bootNr from bootstats

        PersistencyUtils::BootStats bootstats;
        if (!PersistencyUtils::loadBootStats(filesystem, bootstats)) {
            MO_DBG_ERR("cannot load bootstats");
            return false;
        }

        bootstats.bootNr++; //assign new boot number to this run
        if (bootstats.bootNr == 0) { //handle roll-over
            bootstats.bootNr = 1;
        }

        bootNr = bootstats.bootNr;

        if (!PersistencyUtils::storeBootStats(filesystem, bootstats)) {
            MO_DBG_ERR("cannot update bootstats");
            return false;
        }
    }

    uptime.bootNr = bootNr;
    unixTime.bootNr = bootNr;

    return true;
}

void Clock::loop() {

    auto platformTimeMs = context.getTicksMs();
    auto platformDeltaMs = platformTimeMs - lastIncrement;

    /* Roll-over protection: if the platform implementation of mocpp_tick_ms does not
     * fill up the full 32 bits before rolling over, the unsigned integer arithmetics
     * would break. Handle this case here, because it is a common mistake */
    if (MO_CLOCK_ROLLOVER_PROTECTION && platformTimeMs < lastIncrement) {
        // rolled over
        if (platformDeltaMs > 3600 * 1000) {
            // timer rolled over and delta is above 1 hour - that's suspicious
            MO_DBG_ERR("getTickMs roll-over error. Must increment milliseconds up to (32^2 - 1) before resetting to 0");
            platformDeltaMs = 0;
        }
    }

#if MO_ENABLE_TIMESTAMP_MILLISECONDS
    {
        if (platformDeltaMs > (uint32_t)std::numeric_limits<int32_t>::max()) {
            MO_DBG_ERR("integer overflow");
            platformDeltaMs = 0;
        }
        addMs(unixTime, (int32_t)platformDeltaMs);
        addMs(uptime, (int32_t)platformDeltaMs);
        lastIncrement = platformTimeMs;
    }
#else
    {
        auto platformDeltaS = platformDeltaMs / (uint32_t)1000;
        add(unixTime, platformDeltaS);
        add(uptime, platformDeltaS);
        lastIncrement += platformDeltaS * 1000;
    }
#endif //MO_ENABLE_TIMESTAMP_MILLISECONDS
}

const Timestamp &Clock::now() {
    if (unixTime.isUnixTime()) {
        return unixTime;
    } else {
        return uptime;
    }
}

const Timestamp &Clock::getUptime() {
    return uptime;
}

int32_t Clock::getUptimeInt() {
    return uptime.time;
}

bool Clock::setTime(const char* jsonDateString) {

    Timestamp t;
    if (!parseString(jsonDateString, t)) {
        return false;
    }

    unixTime = t;
    return true;
}

bool Clock::setTime(int32_t unixTimeInt) {

    Timestamp t;
    if (!fromUnixTime(t, unixTimeInt)) {
        return false;
    }

    unixTime = t;
    return true;
}

bool Clock::delta(const Timestamp& t2, const Timestamp& t1, int32_t& dt) const {
    //dt = t2 - t1
    if (!t1.isDefined() || !t2.isDefined()) {
        return false;
    }

    if (t1.isUnixTime() && t2.isUnixTime()) {
        dt = t2.time - t1.time;
        return true;
    } else if (t1.isUptime() && t2.isUptime() && t1.bootNr == t2.bootNr) {

        if ((t1.time > 0 && t2.time - t1.time > t2.time) ||
                (t1.time < 0 && t2.time - t1.time < t2.time)) {
            MO_DBG_ERR("integer overflow");
            return false;
        }

        dt = t2.time - t1.time;
        return true;
    } else {
        Timestamp t1Unix, t2Unix;
        if (toUnixTime(t1, t1Unix) && toUnixTime(t2, t2Unix)) {
            dt = t2Unix.time - t1Unix.time;
            return true;
        } else {
            MO_DBG_ERR("cannot get delta: time captured before initial clock sync");
            return false;
        }
    }
}

bool Clock::add(Timestamp& t, int32_t secs) const {

    if (!t.isDefined()) {
        return false;
    }

    if ((secs > 0 && t.time + secs < t.time) ||
            (secs < 0 && t.time + secs > t.time)) {
        MO_DBG_ERR("integer overflow");
        return false;
    }

    if (t.isUnixTime() && (t.time + secs < MO_MIN_TIME || t.time + secs > MO_MAX_TIME)) {
        MO_DBG_ERR("exceed valid unix time range");
        return false;
    }

    if (t.isUptime() && (t.time + secs > MO_MAX_UPTIME)) {
        MO_DBG_ERR("exceed valid uptime time range");
        return false;
    }

    t.time += secs;

    return true;
}

#if MO_ENABLE_TIMESTAMP_MILLISECONDS
bool Clock::deltaMs(const Timestamp& t2, const Timestamp& t1, int32_t& dtMs) const {

    if (t1.ms < 0 || t1.ms >= 1000 || t2.ms < 0 || t2.ms >= 1000) {
        MO_DBG_ERR("invalid timestamp");
        return false;
    }

    auto fullSec2 = t2;
    fullSec2.ms = 0;

    auto fullSec1 = t1;
    fullSec1.ms = 0;

    int32_t dtS;
    if (!delta(fullSec2, fullSec1, dtS)) {
        return false;
    }

    if ((dtS > 0 && dtS * 1000 + 1000 < dtS) ||
            (dtS < 0 && dtS * 1000 - 1000 > dtS)) {
        MO_DBG_ERR("integer overflow");
        return false;
    }

    dtMs = dtS * 1000 + (int32_t)(t2.ms - t1.ms);
    return true;
}

bool Clock::addMs(Timestamp& t, int32_t ms) const {

    if (t.ms < 0 || t.ms >= 1000) {
        MO_DBG_ERR("invalid timestamp");
        return false;
    }

    if (ms + t.ms < ms) {
        MO_DBG_ERR("integer overflow");
        return false;
    }
    ms += t.ms;

    int32_t s = 0;
    if (ms >= 0) {
        //positive, addition
        s = ms / 1000;
        ms %= 1000;
    } else {
        //negative, substraction
        s = ms / 1000;
        ms -= s * 1000;
        if (ms != 0) {
            s -= 1;
            ms += 1000;
        }
    }

    if (!add(t, s)) {
        return false;
    }

    t.ms = ms;
    return true;
}
#endif //MO_ENABLE_TIMESTAMP_MILLISECONDS

bool Clock::toJsonString(const Timestamp& src, char *dst, size_t size) const {

    if (!src.isDefined()) {
        return false;
    }

    if (size < MO_JSONDATE_SIZE) {
        return false;
    }

    Timestamp t;
    if (!toUnixTime(src, t)) {
        MO_DBG_ERR("timestamp not a unix time");
        return false;
    }

    int32_t time = t.time;

    int year = 1970;
    int month = 0;
    while (time - (noDays(month, year) * 24 * 3600) >= 0) {
        time -= noDays(month, year) * 24 * 3600;
        month++;
        if (month >= 12) {
            year++;
            month = 0;
        }
    }

    int day = time / (24 * 3600);
    time %= 24 * 3600;
    int hour = time / 3600;
    time %= 3600;
    int minute = time / 60;
    time %= 60;
    int second = time;

    // first month of the year is '1' and first day of the month is '1'
    month++;
    day++;

    int ret;
#if MO_ENABLE_TIMESTAMP_MILLISECONDS
    ret = snprintf(dst, size, "%04i-%02i-%02iT%02i:%02i:%02i.%03uZ",
            year, month, day, hour, minute, second, (int)(t.ms >= 0 && t.ms <= 999 ? t.ms : 0));
#else
    ret = snprintf(dst, size, "%04i-%02i-%02iT%02i:%02i:%02iZ",
            year, month, day, hour, minute, second);
#endif //MO_ENABLE_TIMESTAMP_MILLISECONDS

    if (ret < 0 || (size_t)ret >= size) {
        return false;
    }

    //success
    return true;
}

bool Clock::toInternalString(const Timestamp& t, char *dst, size_t size) const {

    if (!t.isDefined()) {
        return false;
    }

    if (size < MO_INTERNALTIME_SIZE) {
        return false;
    }

    int ret;
#if MO_ENABLE_TIMESTAMP_MILLISECONDS
    ret = snprintf(dst, size, "i%ut%lim%u", (unsigned int)t.bootNr, (long int)t.time, (int)(t.ms >= 0 && t.ms <= 999 ? t.ms : 0));
#else
    ret = snprintf(dst, size, "i%ut%li", (unsigned int)t.bootNr, (long int)t.time);
#endif //MO_ENABLE_TIMESTAMP_MILLISECONDS

    if (ret < 0 || (size_t)ret >= size) {
        return false;
    }

    return true;
}

bool Clock::parseString(const char *src, Timestamp& dst) const {

    if (src[0] == 'i') { //this is the internal representation, consisting of an optional bootNr and a time value
        size_t i = 1;
        uint16_t bootNr = 0, bootNr2 = 0;
        for (; isdigit(src[i]); i++) {
            bootNr *= 10;
            bootNr += src[i] - '0';
            if (bootNr < bootNr2) {
                //overflow
                MO_DBG_ERR("invalid bootNr");
                return false;
            }
            bootNr2 = bootNr;
        }

        if (src[i++] != 't') {
            MO_DBG_ERR("invalid time string");
            return false;
        }

        bool positive = true;
        if (src[i] == '-') {
            positive = false;
            i++;
        }

        int32_t time = 0, time2 = 0;
        for (; isdigit(src[i]); i++) {
            time *= 10;
            if (positive) {
                time += src[i] - '0';
            } else {
                time -= src[i] - '0';
            }
            if ((positive && time < time2) || (!positive && time > time2)) {
                //overflow
                MO_DBG_ERR("invalid time");
                return false;
            }
            if (time >= MO_MAX_TIME) {
                //exceed numeric limit
                MO_DBG_ERR("invalid time");
                return false;
            }
            if (time < MO_MIN_TIME && time > MO_MAX_UPTIME) {
                MO_DBG_ERR("invalid time");
                return false;
            }
            time2 = time;
        }

        // Optional ms
        int16_t ms = 0;
        if (src[i] == 'm') {
            i++;
            for (; isdigit(src[i]); i++) {
                ms *= 10;
                ms += src[i] - '0';
                if (ms >= 1000) {
                    //overflow
                    MO_DBG_ERR("invalid ms");
                    return false;
                }
            }
        }

        if (src[i] != '\0') {
            MO_DBG_ERR("invalid time");
            return false;
        }

        //success
        dst.time = time;
        dst.bootNr = bootNr;
#if MO_ENABLE_TIMESTAMP_MILLISECONDS
        dst.ms = ms;
#endif //MO_ENABLE_TIMESTAMP_MILLISECONDS
        return true;
    } else { // this is a JSON time string

        const int JSONDATE_MINLENGTH = 19;

        if (strlen(src) < JSONDATE_MINLENGTH){
            return false;
        }

        if (!isdigit(src[0]) ||  //2
            !isdigit(src[1]) ||    //0
            !isdigit(src[2]) ||    //1
            !isdigit(src[3]) ||    //3
            src[4] != '-' ||       //-
            !isdigit(src[5]) ||    //0
            !isdigit(src[6]) ||    //2
            src[7] != '-' ||       //-
            !isdigit(src[8]) ||    //0
            !isdigit(src[9]) ||    //1
            src[10] != 'T' ||      //T
            !isdigit(src[11]) ||   //2
            !isdigit(src[12]) ||   //0
            src[13] != ':' ||      //:
            !isdigit(src[14]) ||   //5
            !isdigit(src[15]) ||   //3
            src[16] != ':' ||      //:
            !isdigit(src[17]) ||   //3
            !isdigit(src[18])) {   //2
                                            //ignore subsequent characters
            return false;
        }

        int year  =  (src[0] - '0') * 1000 +
                    (src[1] - '0') * 100 +
                    (src[2] - '0') * 10 +
                    (src[3] - '0');
        int month =  (src[5] - '0') * 10 +
                    (src[6] - '0') - 1;
        int day   =  (src[8] - '0') * 10 +
                    (src[9] - '0') - 1;
        int hour  =  (src[11] - '0') * 10 +
                    (src[12] - '0');
        int minute = (src[14] - '0') * 10 +
                    (src[15] - '0');
        int second = (src[17] - '0') * 10 +
                    (src[18] - '0');

        //optional fractals
        int16_t ms = 0;
        if (src[19] == '.') {
            if (isdigit(src[20]) ||   //1
                isdigit(src[21]) ||   //2
                isdigit(src[22])) {

                ms  =  (src[20] - '0') * 100 +
                        (src[21] - '0') * 10 +
                        (src[22] - '0');
            } else {
                return false;
            }
        }

        if (year < 1970 || year >= 2038 ||
            month < 0 || month >= 12 ||
            day < 0 || day >= noDays(month, year) ||
            hour < 0 || hour >= 24 ||
            minute < 0 || minute >= 60 ||
            second < 0 || second > 60 || //tolerate leap seconds -- (23:59:60) can be a valid time
            ms < 0 || ms >= 1000) {
            return false;
        }

        int32_t time = 0;

        for (int y = 1970; y < year; y++) {
            for (int m = 0; m < 12; m++) {
                time += noDays(m, y) * 24 * 3600;
            }
        }

        for (int m = 0; m < month; m++) {
            time += noDays(m, year) * 24 * 3600;
        }

        time += day * 24 * 3600;
        time += hour * 3600;
        time += minute * 60;
        time += second;

        if (time < MO_MIN_TIME || time > MO_MAX_TIME) {
            MO_DBG_ERR("only accept time range from year 2010 to 2037");
            return false;
        }

        //success
        dst.time = time;
        dst.bootNr = unixTime.bootNr; //set bootNr to a defined value
#if MO_ENABLE_TIMESTAMP_MILLISECONDS
        dst.ms = ms;
#endif //MO_ENABLE_TIMESTAMP_MILLISECONDS
        return true;
    }
}

bool Clock::toUnixTime(const Timestamp& src, Timestamp& dst) const {
    if (src.isUnixTime()) {
        dst = src;
        return true;
    }
    if (!src.isUptime() || !src.isDefined()) {
        return false;
    }

    if (!unixTime.isUnixTime()) {
        //clock doesn't know unix time yet - no conversion is possible
        return false;
    }

    if (src.bootNr != uptime.bootNr) {
        //src is from a previous power cycle - don't have a record of previous boot-time-unix-time offsets
        return false;
    }

    int32_t deltaUptime;
    if (!delta(src, uptime, deltaUptime)) {
        return false;
    }

    dst = unixTime;
    if (!add(dst, deltaUptime)) {
        return false;
    }

    //success
    return true;
}

bool Clock::toUnixTime(const Timestamp& src, int32_t& dst) const {
    Timestamp t;
    if (!toUnixTime(src, t)) {
        return false;
    }
    dst = t.time;
    return true;
}

bool Clock::fromUnixTime(Timestamp& dst, int32_t unixTimeInt) const {

    Timestamp t = unixTime;
    t.time = unixTimeInt;

    if (!t.isUnixTime()) {
        return false;
    }

#if MO_ENABLE_TIMESTAMP_MILLISECONDS
    t.ms = 0;
#endif //MO_ENABLE_TIMESTAMP_MILLISECONDS

    dst = t;
    return true;
}

uint16_t Clock::getBootNr() const {
    return uptime.bootNr;
}
