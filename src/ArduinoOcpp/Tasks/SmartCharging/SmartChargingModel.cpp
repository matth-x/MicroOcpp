// matth-x/ArduinoOcpp
// Copyright Matthias Akstaller 2019 - 2021
// MIT License

#include <ArduinoOcpp/Tasks/SmartCharging/SmartChargingModel.h>
#include <ArduinoOcpp/Core/OcppEngine.h>
#include <Variants.h>

#include <string.h>

using namespace ArduinoOcpp;

const OcppTimestamp ArduinoOcpp::MIN_TIME = OcppTimestamp(2010, 0, 0, 0, 0, 0);
const OcppTimestamp ArduinoOcpp::MAX_TIME = OcppTimestamp(2037, 0, 0, 0, 0, 0);

ChargingSchedulePeriod::ChargingSchedulePeriod(JsonObject *json){
    startPeriod = (*json)["startPeriod"];
    limit = (*json)["limit"]; //one fractural digit at most
    numberPhases = (*json)["numberPhases"] | -1;
}
ChargingSchedulePeriod::ChargingSchedulePeriod(int startPeriod, float limit){
    this->startPeriod = startPeriod;
    this->limit = limit;
    numberPhases = -1; //support later
}
int ChargingSchedulePeriod::getStartPeriod(){
    return this->startPeriod;
}
float ChargingSchedulePeriod::getLimit(){
    return this->limit;
}
void ChargingSchedulePeriod::scale(float factor){
    limit *= factor;
}
void ChargingSchedulePeriod::add(float value){
    limit += value;
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
    startSchedule = OcppTimestamp();
    if (!startSchedule.setTime((*json)["startSchedule"] | "Invalid")) {
        //non-success
        startSchedule = MIN_TIME;
    }
    const char *unit = (*json)["scheduleUnit"] | "__Invalid";
    if (unit[0] == 'a' || unit[0] == 'A') {
        schedulingUnit = 'A';
    } else {
        schedulingUnit = 'W';
    }
    
    chargingSchedulePeriod = std::vector<ChargingSchedulePeriod*>();
    JsonArray periodJsonArray = (*json)["chargingSchedulePeriod"];
    for (JsonObject periodJson : periodJsonArray) {
        ChargingSchedulePeriod *period = new ChargingSchedulePeriod(&periodJson);
        chargingSchedulePeriod.push_back(period);
    }

    //Expecting sorted list of periods but specification doesn't garantuee it
    std::sort(chargingSchedulePeriod.begin(), chargingSchedulePeriod.end(), [] (ChargingSchedulePeriod *p1, ChargingSchedulePeriod *p2) {
        return p1->getStartPeriod() < p2->getStartPeriod();
    });
    
    minChargingRate = (*json)["minChargingRate"] | -1.0f;
}

ChargingSchedule::ChargingSchedule(ChargingSchedule &other) {
    chargingProfileKind = other.chargingProfileKind;
    recurrencyKind = other.recurrencyKind;  
    duration = other.duration;
    startSchedule = other.startSchedule;
    schedulingUnit = other.schedulingUnit;
    
    chargingSchedulePeriod = std::vector<ChargingSchedulePeriod*>();

    for (auto period = other.chargingSchedulePeriod.begin(); period != other.chargingSchedulePeriod.end(); period++) {
        ChargingSchedulePeriod *p = new ChargingSchedulePeriod(**period);
        chargingSchedulePeriod.push_back(p);
    }

    //Expecting sorted list of periods but specification doesn't garantuee it
    std::sort(chargingSchedulePeriod.begin(), chargingSchedulePeriod.end(), [] (ChargingSchedulePeriod *p1, ChargingSchedulePeriod *p2) {
        return p1->getStartPeriod() < p2->getStartPeriod();
    });
    
    minChargingRate = other.minChargingRate;
}

ChargingSchedule::ChargingSchedule(OcppTimestamp &startT, int duration) {
    //create empty but valid Charging Schedule
    this->duration = duration;
    startSchedule = startT;
    schedulingUnit = 'W'; //either 'A' or 'W'
    std::vector<ChargingSchedulePeriod*> chargingSchedulePeriod = std::vector<ChargingSchedulePeriod*>();
    float minChargingRate = 0.f;

    chargingProfileKind = ChargingProfileKindType::Absolute; //copied from ChargingProfile to increase cohesion of limit inferencing methods
    recurrencyKind = RecurrencyKindType::NOT_SET; //copied from ChargingProfile to increase cohesion of limit inferencing methods
}

ChargingSchedule::~ChargingSchedule(){
    for (size_t i = 0; i < chargingSchedulePeriod.size(); i++) {
        delete chargingSchedulePeriod.at(i);
    }
}

float maximum(float f1, float f2){
    if (f1 < f2) {
        return f2;
    } else {
        return f1;
    }
}

bool ChargingSchedule::inferenceLimit(const OcppTimestamp &t, const OcppTimestamp &startOfCharging, float *limit, OcppTimestamp *nextChange) {
    OcppTimestamp basis = OcppTimestamp(); //point in time to which schedule-related times are relative
    *nextChange = MAX_TIME; //defaulted to Infinity
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
            if (startSchedule > MIN_TIME) {
                basis = startSchedule;
            } else if (startOfCharging > MIN_TIME && startOfCharging < t) {
                basis = startOfCharging;
            } else {
                Serial.print(F("[SmartChargingModel] Undefined behaviour: Inferencing limit from absolute profile, but neither startSchedule, nor start of charging are set! Abort\n"));
                return false;
            }
            break;
        case (ChargingProfileKindType::Recurring):
            if (recurrencyKind == RecurrencyKindType::Daily) {
                basis = t - ((t - startSchedule) % (24 * 3600));
                *nextChange = basis + (24 * 3600); //constrain nextChange to basis + one day
            } else if (recurrencyKind == RecurrencyKindType::Weekly) {
                basis = t - ((t - startSchedule) % (7 * 24 * 3600));
                *nextChange = basis + (7 * 24 * 3600);
            } else {
                Serial.print(F("[SmartChargingModel] Undefined behaviour: Recurring ChargingProfile but no RecurrencyKindType set! Assume \"Daily\""));
                basis = t - ((t - startSchedule) % (24 * 3600));
                *nextChange = basis + (24 * 3600);
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
    
    int t_toBasis = t - basis;

    if (duration > 0){
        //duration is set

        //check if duration is exceeded and if yes, abort inferencing limit
        //if no, the duration is an upper limit for the validity of the schedule
        if (t_toBasis >= duration) { //"duration" is given relative to basis
            return false;
        } else {
            if (*nextChange - basis > duration) {
                *nextChange = basis + duration;
            } 
        }
    }

    /*
    * Work through the ChargingProfilePeriods here. If the right period was found, assign the limit parameter from it
    * and make nextChange equal the beginning of the following period. If the right period is the last one, nextChange
    * will remain the time determined before.
    */
    float limit_res = -1.0f; //If limit_res is still -1 after the loop, the inference process failed
    for (auto period = chargingSchedulePeriod.begin(); period != chargingSchedulePeriod.end(); period++) {
        if ((*period)->getStartPeriod() > t_toBasis) {
            // found the first period that comes after t_toBasis.
            *nextChange = basis + (*period)->getStartPeriod();
            break; //The currently valid limit was set the iteration before
        }
        limit_res = (*period)->getLimit();

    }
    
    if (limit_res >= 0.0f) {
        *limit = maximum(limit_res, minChargingRate);
        return true;
    } else {
        return false; //No limit was found. Either there is no ChargingProfilePeriod, or each period begins after t_toBasis
    }
}

bool ChargingSchedule::addChargingSchedulePeriod(ChargingSchedulePeriod *period) {
    if (period == NULL) return false;

    if (period->getStartPeriod() >= duration) {
        return false;
    }

    chargingSchedulePeriod.push_back(period);
    return true;
}

void ChargingSchedule::scale(float factor) {
    for (auto p = chargingSchedulePeriod.begin(); p != chargingSchedulePeriod.end(); p++) {
        (*p)->scale(factor);
    }
}

void ChargingSchedule::translate(float offset) {
    for (auto p = chargingSchedulePeriod.begin(); p != chargingSchedulePeriod.end(); p++) {
        (*p)->add(offset);
    }
}

DynamicJsonDocument *ChargingSchedule::toJsonDocument() {
    size_t capacity = 0;
    capacity += JSON_OBJECT_SIZE(5); //no of fields of ChargingSchedule
    capacity += JSONDATE_LENGTH + 1; //startSchedule
    capacity += 2; //scheduling unit will be saved as string
    capacity += JSON_ARRAY_SIZE(chargingSchedulePeriod.size()) + chargingSchedulePeriod.size() * JSON_OBJECT_SIZE(3);

    DynamicJsonDocument *result = new DynamicJsonDocument(capacity);
    JsonObject payload = result->to<JsonObject>();
    if (duration >= 0)
        payload["duration"] = duration;
    char startScheduleJson [JSONDATE_LENGTH + 1] = {'\0'};
    startSchedule.toJsonString(startScheduleJson, JSONDATE_LENGTH + 1);
    payload["startSchedule"] = startScheduleJson;
    payload["schedulingUnit"] = schedulingUnit;
    JsonArray periodArray = payload.createNestedArray("chargingSchedulePeriod");
    for (auto period = chargingSchedulePeriod.begin(); period != chargingSchedulePeriod.end(); period++) {
        JsonObject entry = periodArray.createNestedObject();
        entry["startPeriod"] = (*period)->getStartPeriod();
        entry["limit"] = (*period)->getLimit();
        if ((*period)->getNumberPhases() >= 0) {
            entry["numberPhases"] = (*period)->getNumberPhases();
        }
    }
    if (minChargingRate >= 0)
        payload["minChargeRate"] = minChargingRate;
    
    return result;
}

void ChargingSchedule::printSchedule(){
    Serial.print(F("    duration: "));
    Serial.print(duration);
    Serial.print(F("\n"));
    
    Serial.print(F("    startSchedule: "));
    char tmp[JSONDATE_LENGTH + 1] = {'\0'};
    startSchedule.toJsonString(tmp, JSONDATE_LENGTH + 1);
    Serial.print(tmp);
    Serial.print(F("\n"));

    Serial.print(F("    schedulingUnit: "));
    Serial.print(schedulingUnit);
    Serial.print(F("\n"));

    for (auto period = chargingSchedulePeriod.begin(); period != chargingSchedulePeriod.end(); period++) {
        (*period)->printPeriod();
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

    if (!validFrom.setTime((*json)["validFrom"] | "Invalid")) {
        //non-success
        Serial.print(F("[SmartChargingModel] Error reading validFrom. Expect format like 2020-02-01T20:53:32.486Z. Assume unlimited validity\n"));
        validFrom = MIN_TIME;
    }

    if (!validTo.setTime((*json)["validTo"] | "Invalid")) {
        //non-success
        Serial.print(F("[SmartChargingModel] Error reading validTo. Expect format like 2020-02-01T20:53:32.486Z. Assume unlimited validity\n"));
        validTo = MIN_TIME;
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

bool ChargingProfile::inferenceLimit(const OcppTimestamp &t, const OcppTimestamp &startOfCharging, float *limit, OcppTimestamp *nextChange){
    if (t > validTo && validTo > MIN_TIME) {
        *nextChange = MAX_TIME;
        return false; //no limit defined
    }
    if (t < validFrom) {
        *nextChange = validFrom;
        return false; //no limit defined
    }

    return chargingSchedule->inferenceLimit(t, startOfCharging, limit, nextChange);
}

bool ChargingProfile::inferenceLimit(const OcppTimestamp &t, float *limit, OcppTimestamp *nextChange){
    return inferenceLimit(t, MAX_TIME, limit, nextChange);
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

int ChargingProfile::getChargingProfileId() {
    return chargingProfileId;
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
    char tmp[JSONDATE_LENGTH + 1] = {'\0'};
    validFrom.toJsonString(tmp, JSONDATE_LENGTH + 1);
    Serial.print(F("\n"));

    Serial.print(F("  validTo: "));
    validTo.toJsonString(tmp, JSONDATE_LENGTH + 1);
    Serial.print(F("\n"));
    chargingSchedule->printSchedule();
}
