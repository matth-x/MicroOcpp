#ifndef TIMEHELPER_H
#define TIMEHELPER_H

#include <TimeLib.h>

//Infinity begins in the end of year 2035
//... it is close to the "year 2038 problem" that occurs if signed longs were used for time_t but has some space
//left to prevent overflows if something is added to INFINITY
//As long as it is garantueed that time_t uses unsigned longs, CalendarYrToTm(2036) could be raised to CalenderYrToTm(2100)
#define INFINITY_THLD ((time_t) (((time_t) SECS_PER_YEAR) * ((time_t) CalendarYrToTm(2036))))
#define INFINITY_STRING "2036-01-01T00:00:00.000Z"


#define JSONDATE_LENGTH 24 //JSON Date
//#define JSONDATE_LENGTH 32 //ISO 8601

bool setTimeFromJsonDateString(const char* jsonDateString);

bool getJsonDateStringFromSystemTime(char* jsonDateString, int bufflength);

bool getJsonDateStringFromGivenTime(char* jsonDateString, int bufflength, time_t t);

bool getTimeFromJsonDateString(const char* jsonDateString, time_t *result);

time_t minimum(time_t t1, time_t t2);

void printTime(time_t t);

#endif
