// matth-x/ESP8266-OCPP
// Copyright Matthias Akstaller 2019 - 2021
// MIT License

#include <ArduinoOcpp/Tasks/SmartCharging/SmartChargingModel.h>
#include <Variants.h>

#include <string.h>

using namespace ArduinoOcpp;

ChargingSchedulePeriod::ChargingSchedulePeriod(JsonObject *json){
  startPeriod = (*json)["startPeriod"];
  limit = (*json)["limit"]; //one fractural digit at most
  numberPhases = (*json)["numberPhases"] | -1;
}
int ChargingSchedulePeriod::getStartPeriod(){
  return this->startPeriod;
}
float ChargingSchedulePeriod::getLimit(){
  return this->limit;
}
int ChargingSchedulePeriod::getNumberPhases(){
  Serial.print(F("[SmartChargingModel] Unsupported operation: ChargingSchedulePeriod::getNumberPhases(); No phase control implemented"));
  return this->numberPhases;
}

void ChargingSchedulePeriod::printPeriod(){
  Serial.print(F("      startPeriod: "));
  Serial.print(startPeriod);
  Serial.print(F("\n"));

  Serial.print(F("      limit: "));
  Serial.print(limit);
  Serial.print(F("\n"));

  Serial.print(F("      numberPhases: "));
  Serial.print(numberPhases);
  Serial.print(F("\n"));
}

ChargingSchedule::ChargingSchedule(JsonObject *json, ChargingProfileKindType chargingProfileKind, RecurrencyKindType recurrencyKind)
      : chargingProfileKind(chargingProfileKind)
      , recurrencyKind(recurrencyKind) {  
  
  duration = (*json)["duration"] | -1;
  if (!getTimeFromJsonDateString((*json)["startSchedule"] | "1970-01-01T00:00:00.000Z", &startSchedule)){
    //non-success
  }
  schedulingUnit = (*json)["scheduleUnit"] | 'W'; //either 'A' or 'W'
  
  chargingSchedulePeriod = LinkedList<ChargingSchedulePeriod*>();
  JsonArray periodJsonArray = (*json)["chargingSchedulePeriod"];
  for (JsonObject periodJson : periodJsonArray) {
    ChargingSchedulePeriod *period = new ChargingSchedulePeriod(&periodJson);
    chargingSchedulePeriod.add(period);
  }

  //Expecting sorted list of periods but specification doesn't garantuee it
  chargingSchedulePeriod.sort([] (ChargingSchedulePeriod *&p1, ChargingSchedulePeriod *&p2) {
    return p1->getStartPeriod() - p2->getStartPeriod();
  });
  
  minChargingRate = (*json)["minChargingRate"] | -1.0f;
}

ChargingSchedule::~ChargingSchedule(){
  for (int i = 0; i < chargingSchedulePeriod.size(); i++) {
    delete chargingSchedulePeriod.get(i);
  }
  chargingSchedulePeriod.clear();
}

float maximum(float f1, float f2){
  if (f1 < f2) {
    return f2;
  } else {
    return f1;
  }
}

bool ChargingSchedule::inferenceLimit(time_t t, time_t startOfCharging, float *limit, time_t *nextChange) {
  time_t basis; //point in time to which schedule-related times are relative
  *nextChange = INFINITY_THLD; //defaulted to Infinity
  switch (chargingProfileKind) {
    case (ChargingProfileKindType::Absolute):
      //check if schedule is not valid yet but begins in future
      if (startSchedule > t) {
        //not valid YET
        *nextChange = startSchedule;
        return false;
      }
      //If charging profile is absolute, prefer startSchedule as basis. If absent, use chargingStart instead. If absent, no
      //behaviour is defined
      if (startSchedule > 0) {
        basis = startSchedule;
      } else if (startOfCharging > 0 && startOfCharging < t) {
        basis = startOfCharging;
      } else {
        Serial.print(F("[SmartChargingModel] Undefined behaviour: Inferencing limit from absolute profile, but neither startSchedule, nor start of charging are set! Abort\n"));
        return false;
      }
      break;
    case (ChargingProfileKindType::Recurring):
      if (recurrencyKind == RecurrencyKindType::Daily) {
        basis = t - ((t - startSchedule) % SECS_PER_DAY);
        *nextChange = basis + SECS_PER_DAY; //constrain nextChange to basis + one day
      } else if (recurrencyKind == RecurrencyKindType::Weekly) {
        basis = t - ((t - startSchedule) % SECS_PER_WEEK);
        *nextChange = basis + SECS_PER_WEEK;
      } else {
        Serial.print(F("[SmartChargingModel] Undefined behaviour: Recurring ChargingProfile but no RecurrencyKindType set! Assume \"Daily\""));
        basis = t - ((t - startSchedule) % SECS_PER_DAY);
        *nextChange = basis + SECS_PER_DAY;
      }
      break;
    case (ChargingProfileKindType::Relative):
      //assumed, that it is relative to start of charging
      //start of charging must be before t or equal to t
      if (startOfCharging > t) {
        //Relative charging profiles only work with a currently active charging session which is not the case here
        return false;
      }
      basis = startOfCharging;
      break;
  }

  if (t < basis) { //check for error
    Serial.print(F("[SmartChargingModel] Error in SchmartChargingModel::inferenceLimit: time basis is smaller than t, but t must be >= basis\n"));
    return false;
  }
  
  time_t t_toBasis = t - basis;

  if (duration > 0){
    //duration is set

    //check if duration is exceeded and if yes, abort inferencing limit
    //if no, the duration is an upper limit for the validity of the schedule
    if (t_toBasis >= duration) { //"duration" is given relative to basis
      return false;
    } else {
      *nextChange = minimum(*nextChange, basis + duration);
    }
  }

  /*
   * Work through the ChargingProfilePeriods here. If the right period was found, assign the limit parameter from it
   * and make nextChange equal the beginning of the following period. If the right period is the last one, nextChange
   * will remain the time determined before.
   */
  float limit_res = -1.0f; //If limit_res is still -1 after the loop, the inference process failed
  for (int i = 0; i < chargingSchedulePeriod.size(); i++){
    if (chargingSchedulePeriod.get(i)->getStartPeriod() > t_toBasis) {
      // found the first period that comes after t_toBasis.
      *nextChange = basis + chargingSchedulePeriod.get(i)->getStartPeriod();
      break; //The currently valid limit was set the iteration before
    }
    limit_res = chargingSchedulePeriod.get(i)->getLimit();
  }
  
  if (limit_res >= 0.0f) {
    *limit = maximum(limit_res, minChargingRate);
    return true;
  } else {
    return false; //No limit was found. Either there is no ChargingProfilePeriod, or each period begins after t_toBasis
  }
}


void ChargingSchedule::printSchedule(){
  Serial.print(F("    duration: "));
  Serial.print(duration);
  Serial.print(F("\n"));
  
  Serial.print(F("    startSchedule: "));
  Serial.print(startSchedule);
  Serial.print(F("\n"));

  Serial.print(F("    schedulingUnit: "));
  Serial.print(schedulingUnit);
  Serial.print(F("\n"));

  for (int i = 0; i < chargingSchedulePeriod.size(); i++){
    chargingSchedulePeriod.get(i)->printPeriod();
  }

  Serial.print(F("    minChargingRate: "));
  Serial.print(minChargingRate);
  Serial.print(F("\n"));
}

ChargingProfile::ChargingProfile(JsonObject *json){
  
  chargingProfileId = (*json)["chargingProfileId"];
  transactionId = (*json)["transactionId"] | -1;
  stackLevel = (*json)["stackLevel"];

  if (DEBUG_OUT) {
    Serial.print(F("[SmartChargingModel] ChargingProfile created with chargingProfileId = "));
    Serial.print(chargingProfileId);
    Serial.print(F("\n"));
  }
  
  const char *chargingProfilePurposeStr = (*json)["chargingProfilePurpose"] | "Invalid";
  if (DEBUG_OUT) {
    Serial.print(F("[SmartChargingModel] chargingProfilePurposeStr="));
    Serial.print(chargingProfilePurposeStr);
    Serial.print(F("\n"));
  }
  if (!strcmp(chargingProfilePurposeStr, "ChargePointMaxProfile")) {
    chargingProfilePurpose = ChargingProfilePurposeType::ChargePointMaxProfile;
  } else if (!strcmp(chargingProfilePurposeStr, "TxDefaultProfile")) {
    chargingProfilePurpose = ChargingProfilePurposeType::TxDefaultProfile;
  //} else if (!strcmp(chargingProfilePurposeStr, "TxProfile")) {
  } else {
    chargingProfilePurpose = ChargingProfilePurposeType::TxProfile;
  }
  const char *chargingProfileKindStr = (*json)["chargingProfileKind"] | "Invalid";
  if (!strcmp(chargingProfileKindStr, "Absolute")) {
    chargingProfileKind = ChargingProfileKindType::Absolute;
  } else if (!strcmp(chargingProfileKindStr, "Recurring")) {
    chargingProfileKind = ChargingProfileKindType::Recurring;
  //} else if (!strcmp(chargingProfileKindStr, "Relative")) {
  } else {
    chargingProfileKind = ChargingProfileKindType::Relative;
  }
  const char *recurrencyKindStr = (*json)["recurrencyKind"] | "Invalid";
  if (DEBUG_OUT) {
    Serial.print(F("[SmartChargingModel] recurrencyKindStr="));
    Serial.print(recurrencyKindStr);
    Serial.print('\n');
  }
  if (!strcmp(recurrencyKindStr, "Daily")) {
    recurrencyKind = RecurrencyKindType::Daily;
  } else if (!strcmp(recurrencyKindStr, "Weekly")) {
    recurrencyKind = RecurrencyKindType::Weekly;
  } else {
    recurrencyKind = RecurrencyKindType::NOT_SET; //not part of OCPP 1.6
  }

  const char *validFromStr = (*json)["validFrom"] | "1970-01-01T00:00:00.000Z";
  if (!getTimeFromJsonDateString(validFromStr, &validFrom)){
    //non-success
    Serial.print(F("[SmartChargingModel] Error reading validFrom. Expect format like 2020-02-01T20:53:32.486Z. Assume year 1970 for now\n"));
  }

  const char *validToStr = (*json)["validTo"] | INFINITY_STRING;
  if (!getTimeFromJsonDateString(validToStr, &validTo)){
    //non-success
    Serial.print(F("[SmartChargingModel] Error reading validTo. Expect format like 2020-02-01T20:53:32.486Z. Assume year 2037 for now\n"));
  }
  
  JsonObject schedule = (*json)["chargingSchedule"]; 
  chargingSchedule = new ChargingSchedule(&schedule, chargingProfileKind, recurrencyKind);
}

ChargingProfile::~ChargingProfile(){
  delete chargingSchedule;
}

/**
 * Modulo, the mathematical way
 * 
 * dividend:    -4  -3  -2  -1   0   1   2   3   4 
 * % divisor:    3   3   3   3   3   3   3   3   3
 * = remainder:  2   0   1   2   0   1   2   0   1
 */
int modulo(int dividend, int divisor){
  int remainder = dividend;
  while (remainder < 0) {
    remainder += divisor;
  }
  while (remainder >= divisor) {
    remainder -= divisor;
  }
  return remainder;
}

bool ChargingProfile::inferenceLimit(time_t t, time_t startOfCharging, float *limit, time_t *nextChange){
  if (t > validTo && validTo > 0) {
    *nextChange = INFINITY_THLD;
    return false; //no limit defined
  }
  if (t < validFrom) {
    *nextChange = validFrom;
    return false; //no limit defined
  }

  return chargingSchedule->inferenceLimit(t, startOfCharging, limit, nextChange);
}

bool ChargingProfile::inferenceLimit(time_t t, float *limit, time_t *nextChange){
  return inferenceLimit(t, INFINITY_THLD, limit, nextChange);
}

bool ChargingProfile::checkTransactionId(int chargingSessionTransactionID) {
  if (transactionId >= 0 && chargingSessionTransactionID >= 0){
    //Transaction IDs are valid
    if (chargingProfilePurpose == ChargingProfilePurposeType::TxProfile //only then a transactionId can restrict the limits
          && transactionId != chargingSessionTransactionID) {
      return false;
    }
  }
  return true;
}

int ChargingProfile::getStackLevel(){
  return stackLevel;
}
  
ChargingProfilePurposeType ChargingProfile::getChargingProfilePurpose(){
  return chargingProfilePurpose;
}

void ChargingProfile::printProfile(){
  Serial.print(F("  chargingProfileId: "));
  Serial.print(chargingProfileId);
  Serial.print(F("\n"));

  Serial.print(F("  transactionId: "));
  Serial.print(transactionId);
  Serial.print(F("\n"));

  Serial.print(F("  stackLevel: "));
  Serial.print(stackLevel);
  Serial.print(F("\n"));

  Serial.print(F("  chargingProfilePurpose: "));
  switch (chargingProfilePurpose) {
    case (ChargingProfilePurposeType::ChargePointMaxProfile):
      Serial.print(F("ChargePointMaxProfile"));
      break;
    case (ChargingProfilePurposeType::TxDefaultProfile):
      Serial.print(F("TxDefaultProfile"));
      break;
    case (ChargingProfilePurposeType::TxProfile):
      Serial.print(F("TxProfile"));
      break;    
  }
  Serial.print(F("\n"));

  Serial.print(F("  chargingProfileKind: "));
  switch (chargingProfileKind) {
    case (ChargingProfileKindType::Absolute):
      Serial.print(F("Absolute"));
      break;
    case (ChargingProfileKindType::Recurring):
      Serial.print(F("Recurring"));
      break;
    case (ChargingProfileKindType::Relative):
      Serial.print(F("Relative"));
      break;    
  }
  Serial.print(F("\n"));

  Serial.print(F("  recurrencyKind: "));
  switch (recurrencyKind) {
    case (RecurrencyKindType::Daily):
      Serial.print(F("Daily"));
      break;
    case (RecurrencyKindType::Weekly):
      Serial.print(F("Weekly"));
      break;
    case (RecurrencyKindType::NOT_SET):
      Serial.print(F("NOT_SET"));
      break;    
  }
  Serial.print(F("\n"));

  Serial.print(F("  validFrom: "));
  printTime(validFrom);
  Serial.print(F("\n"));

  Serial.print(F("  validTo: "));
  printTime(validTo);
  Serial.print(F("\n"));
  chargingSchedule->printSchedule();
}
