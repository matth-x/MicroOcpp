// matth-x/ArduinoOcpp
// Copyright Matthias Akstaller 2019 - 2021
// MIT License

#include <ArduinoOcpp/Core/OcppTime.h>

namespace ArduinoOcpp {

namespace Clocks {

ulong lastClockReading = 0;
otime_t lastClockValue = 0;

/*
 * Basic clock implementation. Works if millis() is exact enough for you and if device doesn't go in sleep mode. 
 */
OcppClock DEFAULT_CLOCK = [] () {
    ulong tReading = (millis() - lastClockReading) / 1000UL;
    if (tReading > 0) {
        lastClockValue += tReading;
        lastClockReading += tReading * 1000UL;
    }
    return lastClockValue;};
} //end namespace Clocks


OcppTimestamp::OcppTimestamp() {
    
}

int noDays(int month, int year) {
    return (month == 0 || month == 2 || month == 4 || month == 6 || month == 7 || month == 9 || month == 11) ? 31 :
            ((month == 3 || month == 5 || month == 8 || month == 10) ? 30 :
            ((year % 4 == 0 && (year % 100 != 0 || year % 400 == 0)) ? 29 : 28));
}

bool OcppTimestamp::setTime(const char *jsonDateString) {

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
    //ignore fractals

    if (year < 1970 || year >= 2038 ||
        month < 0 || month >= 12 ||
        day < 0 || day >= noDays(month, year) ||
        hour < 0 || hour >= 24 ||
        minute < 0 || minute >= 60 ||
        second < 0 || second > 60) { //tolerate leap seconds -- (23:59:60) can be a valid time
        return false;
    }

    this->year = year;
    this->month = month;
    this->day = day;
    this->hour = hour;
    this->minute = minute;
    this->second = second;
    
    return true;
}

bool OcppTimestamp::toJsonString(char *jsonDateString, size_t buffsize) {
    if (buffsize < OCPP_JSONDATE_LENGTH + 1) return false;

    jsonDateString[0] = ((char) ((year / 1000) % 10)) + '0';
    jsonDateString[1] = ((char) ((year / 100) % 10)) + '0';
    jsonDateString[2] = ((char) ((year / 10) % 10))  + '0';
    jsonDateString[3] = ((char) ((year / 1) % 10))  + '0';
    jsonDateString[4] = '-';
    jsonDateString[5] = ((char) ((month / 10) % 10))  + '0';
    jsonDateString[6] = ((char) ((month / 1) % 10))  + '0';
    jsonDateString[7] = '-';
    jsonDateString[8] = ((char) ((day / 10) % 10))  + '0';
    jsonDateString[9] = ((char) ((day / 1) % 10))  + '0';
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
    jsonDateString[20] = '0'; //ignore fractals
    jsonDateString[21] = '0';
    jsonDateString[22] = '0';
    jsonDateString[23] = 'Z';

    return true;
}

OcppTimestamp OcppTimestamp::operator+(int secs) {
    OcppTimestamp res = *this;
    res.second += secs;

    if (res.second >= 0 && res.second < 60) return res;

    res.minute += res.second / 60;
    res.second %= 60;
    if (res.second < 0) {
        res.minute--;
        res.second += 60;
    }

    if (res.minute >= 0 && res.minute < 60) return res;

    res.hour += res.minute / 60;
    res.minute %= 60;
    if (res.minute < 0) {
        res.hour--;
        res.minute += 60;
    }

    if (res.hour >= 0 && res.hour < 24) return res;

    res.day += res.hour / 24;
    res.hour %= 24;
    if (res.hour < 0) {
        res.day--;
        res.hour += 24;
    }

    while (res.day >= noDays(res.month, res.year)) {
        res.day -= noDays(res.month, res.year);
        res.month++;
        
        if (res.month >= 12) {
            res.month -= 12;
            res.year++;
        }
    }

    while (res.day < 0) {
        res.month--;
        if (res.month < 0) {
            res.month += 12;
            res.year--;
        }
        res.day += noDays(res.month, res.year);
    }

    return res;
};

OcppTimestamp OcppTimestamp::operator-(int secs) {
    return this->operator+(-secs);
}

OcppTime::OcppTime(OcppClock system_clock) : system_clock(system_clock) {

}

bool OcppTime::setOcppTime(const char* jsonDateString) {

    OcppTimestamp timestamp = OcppTimestamp();
    
    if (!timestamp.setTime(jsonDateString)) {
        return false;
    }

    system_basetime = system_clock();
    ocpp_basetime = timestamp;
    
    return true;
}

otime_t OcppTime::getOcppTimeScalar() {
    return system_clock();
}

OcppTimestamp OcppTime::createTimestamp(otime_t scalar) {
    OcppTimestamp res = ocpp_basetime + (scalar - system_basetime);

    return res;
}

}
