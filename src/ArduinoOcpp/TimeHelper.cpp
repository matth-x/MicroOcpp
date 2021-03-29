// matth-x/ESP8266-OCPP
// Copyright Matthias Akstaller 2019 - 2021
// MIT License

#include <ArduinoOcpp/TimeHelper.h>

#include <Arduino.h>
#include <string.h>
#include <ctype.h>

namespace ArduinoOcpp {

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
bool setTimeFromJsonDateString(const char* jsonDateString){
  time_t mDate = 0;
  bool success = getTimeFromJsonDateString(jsonDateString, &mDate);
  if (success) {
    setTime(mDate);
  }
  return success;
}

bool getTimeFromJsonDateString(const char* jsonDateString, time_t *result){
  
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
               (jsonDateString[6] - '0');
  int day   =  (jsonDateString[8] - '0') * 10 +
               (jsonDateString[9] - '0');
  int hour  =  (jsonDateString[11] - '0') * 10 +
               (jsonDateString[12] - '0');
  int minute = (jsonDateString[14] - '0') * 10 +
               (jsonDateString[15] - '0');
  int second = (jsonDateString[17] - '0') * 10 +
               (jsonDateString[18] - '0');
  //ignore fractals

  if (year < 1970 || year > 2100 ||
      month < 1 || month > 12 ||
      day < 1 || day > 31 ||
      hour < 0 || hour > 23 ||
      minute < 0 || minute > 59 ||
      second < 0 || second > 59) {
    return false;
  }

  /*
   * not-so-sophisticated type-check successfully passed
   */

  TimeElements calendar = {};
  calendar.Second = second;
  calendar.Minute = minute;
  calendar.Hour = hour;
  //uint8_t Wday;   // day of week, sunday is day 1
  calendar.Day = day;
  calendar.Month = month;
  calendar.Year = CalendarYrToTm(year); // CalendarYrToTm(Y) = Y - 1970

  *result = makeTime(calendar);
  
  return true;
}

/**
 * Writes the system time into jsonDateString.
 * 
 * jsonDateString: output buffer. Must be JSONDATE_LENGTH + 1 characters long (+1 for the 0-terminator).
 * Must be 0-initialized.
 * bufflength: number of chars in jsonDateString
 * Warning: writes up to bufflength characters into the buffer
 * Returns true on success, false otherwise
 * 
 * Example usage:
 * char timestamp[JSONDATE_LENGTH + 1] = {'\0'}; //timestamp must not be shorter
 * if (!getJsonDateStringFromSystemTime(timestamp, JSONDATE_LENGTH)){
 *   //catch error
 * }
 * json_document["timestamp"] = timestamp;
 */
bool getJsonDateStringFromSystemTime(char* jsonDateString, int bufflength){
  return getJsonDateStringFromGivenTime(jsonDateString, bufflength, now());
}

/**
 * Writes the system time into jsonDateString.
 * 
 * jsonDateString: output buffer. Must be JSONDATE_LENGTH + 1 characters long (+1 for the 0-terminator).
 * Must be 0-initialized.
 * bufflength: number of chars in jsonDateString
 * Warning: writes up to bufflength characters into the buffer
 * Returns true on success, false otherwise
 * 
 * Example usage:
 * char timestamp[JSONDATE_LENGTH + 1] = {'\0'}; //timestamp must not be shorter
 * if (!getJsonDateStringFromSystemTime(timestamp, JSONDATE_LENGTH)){
 *   //catch error
 * }
 * json_document["timestamp"] = timestamp;
 */
bool getJsonDateStringFromGivenTime(char* jsonDateString, int bufflength, time_t t){
  if (bufflength < JSONDATE_LENGTH) return false;

  jsonDateString[0] = ((char) ((year(t) / 1000) % 10)) + '0';
  jsonDateString[1] = ((char) ((year(t) / 100) % 10)) + '0';
  jsonDateString[2] = ((char) ((year(t) / 10) % 10))  + '0';
  jsonDateString[3] = ((char) ((year(t) / 1) % 10))  + '0';
  jsonDateString[4] = '-';
  jsonDateString[5] = ((char) ((month(t) / 10) % 10))  + '0';
  jsonDateString[6] = ((char) ((month(t) / 1) % 10))  + '0';
  jsonDateString[7] = '-';
  jsonDateString[8] = ((char) ((day(t) / 10) % 10))  + '0';
  jsonDateString[9] = ((char) ((day(t) / 1) % 10))  + '0';
  jsonDateString[10] = 'T';
  jsonDateString[11] = ((char) ((hour(t) / 10) % 10))  + '0';
  jsonDateString[12] = ((char) ((hour(t) / 1) % 10))  + '0';
  jsonDateString[13] = ':';
  jsonDateString[14] = ((char) ((minute(t) / 10) % 10))  + '0';
  jsonDateString[15] = ((char) ((minute(t) / 1) % 10))  + '0';
  jsonDateString[16] = ':';
  jsonDateString[17] = ((char) ((second(t) / 10) % 10))  + '0';
  jsonDateString[18] = ((char) ((second(t) / 1) % 10))  + '0';

  /*
   * Determining the following characters is a little bit difficult. The format is not specified by
   * OCPP and depends strongly on the internal implementation of the server this client should be 
   * connected to.
   * 
   * However, the OCPP-J 1.6 document suggests to stick to the JSON Date format
   * 2013-02-01T20:53:32.486Z
   * 
   * Other possibilities are commented out and could be reused if necessary
   */

  /* 5 fractal digits and timezone specification
  jsonDateString[19] = '.';
  jsonDateString[20] = '0'; //ignore fractals
  jsonDateString[21] = '0';
  jsonDateString[22] = '0';
  jsonDateString[23] = '0';
  jsonDateString[24] = '0';
  jsonDateString[25] = '0';
  jsonDateString[26] = '+';
  jsonDateString[27] = '0'; //timezone set to +00:00
  jsonDateString[28] = '0';
  jsonDateString[29] = ':';
  jsonDateString[30] = '0';
  jsonDateString[31] = '0'; */

  /* No fractal digits and timezone specification
  jsonDateString[19] = '+';
  jsonDateString[20] = '0'; //ignore fractals
  jsonDateString[21] = '0';
  jsonDateString[22] = ':';
  jsonDateString[23] = '0';
  jsonDateString[24] = '0'; */
  

  /* JSON Date variant. 3 fractal digits and subsequent 'Z' */

  jsonDateString[19] = '.';
  jsonDateString[20] = '0'; //ignore fractals
  jsonDateString[21] = '0';
  jsonDateString[22] = '0';
  jsonDateString[23] = 'Z';

  return true;
}

time_t minimum(time_t t1, time_t t2) {
  if (t1 < t2) {
    return t1;
  } else {
    return t2;
  }
}

void printTime(time_t t) {
  Serial.print(year(t));
  Serial.print(F("-"));
  Serial.print(month(t));
  Serial.print(F("-"));
  Serial.print(day(t));
  Serial.print(F(", "));
  Serial.print(hour(t));
  Serial.print(F(":"));
  Serial.print(minute(t));
  Serial.print(F(":"));
  Serial.print(second(t));
}

} //end namespace ArduinoOcpp
