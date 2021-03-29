// matth-x/ESP8266-OCPP
// Copyright Matthias Akstaller 2019 - 2021
// MIT License

#ifndef SMARTCHARGINGMODEL_H
#define SMARTCHARGINGMODEL_H

#include <LinkedList.h>
#include <ArduinoJson.h>
#include <ArduinoOcpp/TimeHelper.h>

namespace ArduinoOcpp {

enum class ChargingProfilePurposeType {
  ChargePointMaxProfile,
  TxDefaultProfile,
  TxProfile
};

enum class ChargingProfileKindType {
  Absolute,
  Recurring,
  Relative
};

enum class RecurrencyKindType {
  NOT_SET, //not part of OCPP 1.6
  Daily,
  Weekly
};

class ChargingSchedulePeriod {
private:
  int startPeriod;
  float limit; //one fractural digit at most
  int numberPhases = -1;
public:
  ChargingSchedulePeriod(JsonObject *json);
  int getStartPeriod();
  float getLimit();
  int getNumberPhases();
  void printPeriod();
};

class ChargingSchedule {
private:
  int duration = -1;
  time_t startSchedule;
  char schedulingUnit; //either 'A' or 'W'
  LinkedList<ChargingSchedulePeriod*> chargingSchedulePeriod;
  float minChargingRate = -1.0f;

  ChargingProfileKindType chargingProfileKind; //copied from ChargingProfile to increase cohesion of limit inferencing methods
  RecurrencyKindType recurrencyKind; //copied from ChargingProfile to increase cohesion of limit inferencing methods
public:
  ChargingSchedule(JsonObject *json, ChargingProfileKindType chargingProfileKind, RecurrencyKindType recurrencyKind);
  ~ChargingSchedule();

  /**
   * limit: output parameter
   * nextChange: output parameter
   * 
   * returns if charging profile defines a limit at time t
   *       if true, limit and nextChange will be set according to this Schedule
   *       if false, only nextChange will be set
   */
  bool inferenceLimit(time_t t, time_t startOfCharging, float *limit, time_t *nextChange);

  /*
   * print on console
   */
  void printSchedule();
};

class ChargingProfile {
private:
  int chargingProfileId;
  int transactionId = -1;
  int stackLevel;
  ChargingProfilePurposeType chargingProfilePurpose;
  ChargingProfileKindType chargingProfileKind; //copied to ChargingSchedule to increase cohesion of limit inferencing methods
  RecurrencyKindType recurrencyKind; // copied to ChargingSchedule to increase cohesion
  time_t validFrom;
  time_t validTo;
  ChargingSchedule *chargingSchedule;
public:
  ChargingProfile(JsonObject *json);
  ~ChargingProfile();

  /**
   * limit: output parameter
   * nextChange: output parameter
   * 
   * returns if charging profile defines a limit at time t
   *       if true, limit and nextChange will be set according to this Schedule
   *       if false, only nextChange will be set
   */
  bool inferenceLimit(time_t t, time_t startOfCharging, float *limit, time_t *nextChange);

  /*
   * Simpler function if startOfCharging is not available. Caution: This likely will differ from inference with startOfCharging
   */
  bool inferenceLimit(time_t t, float *limit, time_t *nextChange);

  /*
   * Check if this profile belongs to transaction with ID transId
   */
  bool checkTransactionId(int transId);

  int getStackLevel();
  
  ChargingProfilePurposeType getChargingProfilePurpose();

  /*
   * print on console
   */
  void printProfile();
};

} //end namespace ArduinoOcpp
#endif
