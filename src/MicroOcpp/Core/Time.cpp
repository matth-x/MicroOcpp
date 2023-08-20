// matth-x/MicroOcpp
// Copyright Matthias Akstaller 2019 - 2023
// MIT License

#include <MicroOcpp/Core/Time.h>
#include <string.h>
#include <ctype.h>	

namespace MicroOcpp {

const Timestamp MIN_TIME = Timestamp(2010, 0, 0, 0, 0, 0);
const Timestamp MAX_TIME = Timestamp(2037, 0, 0, 0, 0, 0);

Timestamp::Timestamp() {
    
}

Timestamp::Timestamp(const Timestamp& other) {
    *this = other;
}

int noDays(int month, int year) {
    return (month == 0 || month == 2 || month == 4 || month == 6 || month == 7 || month == 9 || month == 11) ? 31 :
            ((month == 3 || month == 5 || month == 8 || month == 10) ? 30 :
            ((year % 4 == 0 && (year % 100 != 0 || year % 400 == 0)) ? 29 : 28));
}

bool Timestamp::setTime(const char *jsonDateString) {

    const int JSONDATE_MINLENGTH = 19;

    if (strlen(jsonDateString) < JSONDATE_MINLENGTH){
        return false;
    }

    if (!isdigit(jsonDateString[0]) ||  //2
        !isdigit(jsonDateString[1]) ||    //0
        !isdigit(jsonDateString[2]) ||    //1
        !isdigit(jsonDateString[3]) ||    //3
        jsonDateString[4] != '-' ||       //-
        !isdigit(jsonDateString[5]) ||    //0
        !isdigit(jsonDateString[6]) ||    //2
        jsonDateString[7] != '-' ||       //-
        !isdigit(jsonDateString[8]) ||    //0
        !isdigit(jsonDateString[9]) ||    //1
        jsonDateString[10] != 'T' ||      //T
        !isdigit(jsonDateString[11]) ||   //2
        !isdigit(jsonDateString[12]) ||   //0
        jsonDateString[13] != ':' ||      //:
        !isdigit(jsonDateString[14]) ||   //5
        !isdigit(jsonDateString[15]) ||   //3
        jsonDateString[16] != ':' ||      //:
        !isdigit(jsonDateString[17]) ||   //3
        !isdigit(jsonDateString[18])) {   //2
                                        //ignore subsequent characters
        return false;
    }
    
    int year  =  (jsonDateString[0] - '0') * 1000 +
                (jsonDateString[1] - '0') * 100 +
                (jsonDateString[2] - '0') * 10 +
                (jsonDateString[3] - '0');
    int month =  (jsonDateString[5] - '0') * 10 +
                (jsonDateString[6] - '0') - 1;
    int day   =  (jsonDateString[8] - '0') * 10 +
                (jsonDateString[9] - '0') - 1;
    int hour  =  (jsonDateString[11] - '0') * 10 +
                (jsonDateString[12] - '0');
    int minute = (jsonDateString[14] - '0') * 10 +
                (jsonDateString[15] - '0');
    int second = (jsonDateString[17] - '0') * 10 +
                (jsonDateString[18] - '0');

    //optional fractals
    int ms = 0;
    if (jsonDateString[19] == '.') {
        if (isdigit(jsonDateString[20]) ||   //1
            isdigit(jsonDateString[21]) ||   //2
            isdigit(jsonDateString[22])) {
            
            ms  =  (jsonDateString[20] - '0') * 100 +
                    (jsonDateString[21] - '0') * 10 +
                    (jsonDateString[22] - '0');
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

    this->year = year;
    this->month = month;
    this->day = day;
    this->hour = hour;
    this->minute = minute;
    this->second = second;
    this->ms = ms;
    
    return true;
}

bool Timestamp::toJsonString(char *jsonDateString, size_t buffsize) const {
    if (buffsize < JSONDATE_LENGTH + 1) return false;

    jsonDateString[0] = ((char) ((year / 1000) % 10)) + '0';
    jsonDateString[1] = ((char) ((year / 100) % 10)) + '0';
    jsonDateString[2] = ((char) ((year / 10) % 10))  + '0';
    jsonDateString[3] = ((char) ((year / 1) % 10))  + '0';
    jsonDateString[4] = '-';
    jsonDateString[5] = ((char) (((month + 1) / 10) % 10))  + '0';
    jsonDateString[6] = ((char) (((month + 1) / 1) % 10))  + '0';
    jsonDateString[7] = '-';
    jsonDateString[8] = ((char) (((day + 1) / 10) % 10))  + '0';
    jsonDateString[9] = ((char) (((day + 1) / 1) % 10))  + '0';
    jsonDateString[10] = 'T';
    jsonDateString[11] = ((char) ((hour / 10) % 10))  + '0';
    jsonDateString[12] = ((char) ((hour / 1) % 10))  + '0';
    jsonDateString[13] = ':';
    jsonDateString[14] = ((char) ((minute / 10) % 10))  + '0';
    jsonDateString[15] = ((char) ((minute / 1) % 10))  + '0';
    jsonDateString[16] = ':';
    jsonDateString[17] = ((char) ((second / 10) % 10))  + '0';
    jsonDateString[18] = ((char) ((second / 1) % 10))  + '0';
    jsonDateString[19] = '.';
    jsonDateString[20] = ((char) ((ms / 100) % 10))  + '0';
    jsonDateString[21] = ((char) ((ms / 10) % 10))  + '0';
    jsonDateString[22] = ((char) ((ms / 1) % 10))  + '0';
    jsonDateString[23] = 'Z';
    jsonDateString[24] = '\0';

    return true;
}

Timestamp &Timestamp::operator+=(int secs) {

    second += secs;

    if (second >= 0 && second < 60) return *this;

    minute += second / 60;
    second %= 60;
    if (second < 0) {
        minute--;
        second += 60;
    }

    if (minute >= 0 && minute < 60) return *this;

    hour += minute / 60;
    minute %= 60;
    if (minute < 0) {
        hour--;
        minute += 60;
    }

    if (hour >= 0 && hour < 24) return *this;

    day += hour / 24;
    hour %= 24;
    if (hour < 0) {
        day--;
        hour += 24;
    }

    while (day >= noDays(month, year)) {
        day -= noDays(month, year);
        month++;
        
        if (month >= 12) {
            month -= 12;
            year++;
        }
    }

    while (day < 0) {
        month--;
        if (month < 0) {
            month += 12;
            year--;
        }
        day += noDays(month, year);
    }

    return *this;
};

Timestamp &Timestamp::addMilliseconds(int val) {

    ms += val;

    if (ms >= 0 && ms < 1000) return *this;
    
    auto dsecond = ms / 1000;
    ms %= 1000;
    if (ms < 0) {
        dsecond--;
        ms += 1000;
    }
    return this->operator+=(dsecond);
}

Timestamp &Timestamp::operator-=(int secs) {
    return operator+=(-secs);
}

int Timestamp::operator-(const Timestamp &rhs) const {
    //dt = rhs - mocpp_base
    
    int16_t year_base, year_end;
    if (year <= rhs.year) {
        year_base = year;
        year_end = rhs.year;
    } else {
        year_base = rhs.year;
        year_end = year;
    }

    int16_t lhsDays = day;
    int16_t rhsDays = rhs.day;

    for (int16_t iy = year_base; iy <= year_end; iy++) {
        for (int16_t im = 0; im < 12; im++) {
            if (year > iy || (year == iy && month > im)) {
                lhsDays += noDays(im, iy);
            }
            if (rhs.year > iy || (rhs.year == iy && rhs.month > im)) {
                rhsDays += noDays(im, iy);
            }
        }
    }

    int dt = (lhsDays - rhsDays) * (24 * 3600) + (hour - rhs.hour) * 3600 + (minute - rhs.minute) * 60 + second - rhs.second;
    return dt;
}

Timestamp &Timestamp::operator=(const Timestamp &rhs) {
    year = rhs.year;
    month = rhs.month;
    day = rhs.day;
    hour = rhs.hour;
    minute = rhs.minute;
    second = rhs.second;
    ms = rhs.ms;

    return *this;
}

Timestamp operator+(const Timestamp &lhs, int secs) {
    Timestamp res = lhs;
    res += secs;
    return res;
}

Timestamp operator-(const Timestamp &lhs, int secs) {
    return operator+(lhs, -secs);
}

bool operator==(const Timestamp &lhs, const Timestamp &rhs) {
    return lhs.year == rhs.year && lhs.month == rhs.month && lhs.day == rhs.day && lhs.hour == rhs.hour && lhs.minute == rhs.minute && lhs.second == rhs.second && lhs.ms == rhs.ms;
}

bool operator!=(const Timestamp &lhs, const Timestamp &rhs) {
    return !(lhs == rhs);
}

bool operator<(const Timestamp &lhs, const Timestamp &rhs) {
    if (lhs.year != rhs.year)
        return lhs.year < rhs.year;
    if (lhs.month != rhs.month)
        return lhs.month < rhs.month;
    if (lhs.day != rhs.day)
        return lhs.day < rhs.day;
    if (lhs.hour != rhs.hour)
        return lhs.hour < rhs.hour;
    if (lhs.minute != rhs.minute)
        return lhs.minute < rhs.minute;
    if (lhs.second != rhs.second)
        return lhs.second < rhs.second;
    if (lhs.ms != rhs.ms)
        return lhs.ms < rhs.ms;
    return false;  
}

bool operator<=(const Timestamp &lhs, const Timestamp &rhs) {
    return lhs < rhs || lhs == rhs;
}

bool operator>(const Timestamp &lhs, const Timestamp &rhs) {
    return rhs < lhs;
}

bool operator>=(const Timestamp &lhs, const Timestamp &rhs) {
    return rhs <= lhs;
}


Clock::Clock() {

}

bool Clock::setTime(const char* jsonDateString) {

    Timestamp timestamp = Timestamp();
    
    if (!timestamp.setTime(jsonDateString)) {
        return false;
    }

    system_basetime = mocpp_tick_ms();
    mocpp_basetime = timestamp;

    currentTime = mocpp_basetime;
    lastUpdate = system_basetime;

    return true;
}

const Timestamp &Clock::now() {
    auto tReading = mocpp_tick_ms();
    auto delta = tReading - lastUpdate;

    currentTime.addMilliseconds(delta);
    lastUpdate = tReading;

    return currentTime;
}

Timestamp Clock::adjustPrebootTimestamp(const Timestamp& t) {
    auto systemtime_in = t - Timestamp();
    if (systemtime_in > (int) system_basetime / 1000) {
        return mocpp_basetime;
    }
    return mocpp_basetime - ((int) (system_basetime / 1000) - systemtime_in);
}

}
