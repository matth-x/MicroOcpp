// matth-x/ArduinoOcpp
// Copyright Matthias Akstaller 2019 - 2022
// MIT License

#include <ArduinoOcpp/Tasks/SmartCharging/SmartChargingModel.h>
#include <ArduinoOcpp/Debug.h>

#include <string.h>

using namespace ArduinoOcpp;

ChargingSchedulePeriod::ChargingSchedulePeriod(JsonObject &json){
    startPeriod = json["startPeriod"];
    limit = json["limit"]; //one fractural digit at most
    numberPhases = json["numberPhases"] | -1;
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
    if (limit < 0.f)
        limit *= -1.f;
}

void ChargingSchedulePeriod::add(float value){
    limit += value;
    if (limit < 0.f)
        limit = 0;
}

int ChargingSchedulePeriod::getNumberPhases(){
    return this->numberPhases;
}

void ChargingSchedulePeriod::printPeriod(){
    AO_DBG_INFO("CHARGING SCHEDULE PERIOD:\n" \
                "      startPeriod: %i\n" \
                "      limit: %f\n" \
                "      numberPhases: %i\n",
                startPeriod,
                limit,
                numberPhases);
}

ChargingSchedule::ChargingSchedule(JsonObject &json, ChargingProfileKindType chargingProfileKind, RecurrencyKindType recurrencyKind)
      : chargingProfileKind(chargingProfileKind)
      , recurrencyKind(recurrencyKind) {  
  
    duration = json["duration"] | -1;
    startSchedule = OcppTimestamp();
    if (!startSchedule.setTime(json["startSchedule"] | "Invalid")) {
        //non-success
        startSchedule = MIN_TIME;
    }
    const char *unit = json["chargingRateUnit"] | "__Invalid";
    if (unit[0] == 'a' || unit[0] == 'A') {
        chargingRateUnit = ChargingRateUnitType::Amp;
    } else {
        chargingRateUnit = ChargingRateUnitType::Watt;
    }
    
    JsonArray periodJsonArray = json["chargingSchedulePeriod"];
    for (JsonObject periodJson : periodJsonArray) {
        auto period = std::unique_ptr<ChargingSchedulePeriod>(new ChargingSchedulePeriod(periodJson));
        chargingSchedulePeriod.push_back(std::move(period));
    }

    //Expecting sorted list of periods but specification doesn't garantuee it
    std::sort(chargingSchedulePeriod.begin(), chargingSchedulePeriod.end(),
        [] (const std::unique_ptr<ChargingSchedulePeriod> &p1, const std::unique_ptr<ChargingSchedulePeriod> &p2) {
            return p1->getStartPeriod() < p2->getStartPeriod();
    });
    
    minChargingRate = json["minChargingRate"] | -1.0f;
}

ChargingSchedule::ChargingSchedule(ChargingSchedule &other) {
    chargingProfileKind = other.chargingProfileKind;
    recurrencyKind = other.recurrencyKind;  
    duration = other.duration;
    startSchedule = other.startSchedule;
    chargingRateUnit = other.chargingRateUnit;

    for (auto period = other.chargingSchedulePeriod.begin(); period != other.chargingSchedulePeriod.end(); period++) {
        auto p = std::unique_ptr<ChargingSchedulePeriod>(new ChargingSchedulePeriod(**period));
        chargingSchedulePeriod.push_back(std::move(p));
    }

    //Expecting sorted list of periods but specification doesn't garantuee it
    std::sort(chargingSchedulePeriod.begin(), chargingSchedulePeriod.end(),
        [] (const std::unique_ptr<ChargingSchedulePeriod> &p1, const std::unique_ptr<ChargingSchedulePeriod> &p2) {
            return p1->getStartPeriod() < p2->getStartPeriod();
    });
    
    minChargingRate = other.minChargingRate;
}

ChargingSchedule::ChargingSchedule(const OcppTimestamp &startT, int duration) {
    //create empty but valid Charging Schedule
    this->duration = duration;
    startSchedule = startT;
    chargingRateUnit = ChargingRateUnitType::Watt;
    //float minChargingRate = 0.f;

    chargingProfileKind = ChargingProfileKindType::Absolute; //copied from ChargingProfile to increase cohesion of limit inferencing methods
    recurrencyKind = RecurrencyKindType::NOT_SET; //copied from ChargingProfile to increase cohesion of limit inferencing methods
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
                AO_DBG_ERR("Absolute profile, but neither startSchedule, nor start of charging are set. Undefined behavior, abort");
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
                AO_DBG_ERR("Recurring ChargingProfile but no RecurrencyKindType set. Undefined behavior, assume 'Daily'");
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
        AO_DBG_ERR("time basis is smaller than t, but t must be >= basis");
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
        *limit = std::max(limit_res, minChargingRate);
        return true;
    } else {
        return false; //No limit was found. Either there is no ChargingProfilePeriod, or each period begins after t_toBasis
    }
}

bool ChargingSchedule::addChargingSchedulePeriod(std::unique_ptr<ChargingSchedulePeriod> period) {
    if (!period) return false;

    if (period->getStartPeriod() >= duration) {
        return false;
    }

    chargingSchedulePeriod.push_back(std::move(period));
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
    capacity += JSON_ARRAY_SIZE(chargingSchedulePeriod.size()) + chargingSchedulePeriod.size() * JSON_OBJECT_SIZE(3);

    DynamicJsonDocument *result = new DynamicJsonDocument(capacity);
    JsonObject payload = result->to<JsonObject>();
    if (duration >= 0)
        payload["duration"] = duration;
    char startScheduleJson [JSONDATE_LENGTH + 1] = {'\0'};
    startSchedule.toJsonString(startScheduleJson, JSONDATE_LENGTH + 1);
    payload["startSchedule"] = startScheduleJson;
    payload["chargingRateUnit"] = chargingRateUnit == (ChargingRateUnitType::Amp) ? "A" : "W";
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

    char tmp[JSONDATE_LENGTH + 1] = {'\0'};
    startSchedule.toJsonString(tmp, JSONDATE_LENGTH + 1);

    AO_DBG_INFO("CHARGING SCHEDULE:\n" \
                "    duration: %i\n" \
                "    startSchedule: %s\n" \
                "    chargingRateUnit: %s\n" \
                "    minChargingRate: %f\n",
                duration,
                tmp,
                chargingRateUnit == (ChargingRateUnitType::Amp) ? "A" :
                    chargingRateUnit == (ChargingRateUnitType::Watt) ? "W" : "Error",
                minChargingRate);

    for (auto period = chargingSchedulePeriod.begin(); period != chargingSchedulePeriod.end(); period++) {
        (*period)->printPeriod();
    }
}

ChargingProfile::ChargingProfile(JsonObject &json){
  
    chargingProfileId = json["chargingProfileId"] | -1;
    transactionId = json["transactionId"] | -1;
    stackLevel = json["stackLevel"] | 0;
    
    const char *chargingProfilePurposeStr = json["chargingProfilePurpose"] | "Invalid";
    if (!strcmp(chargingProfilePurposeStr, "ChargePointMaxProfile")) {
        chargingProfilePurpose = ChargingProfilePurposeType::ChargePointMaxProfile;
    } else if (!strcmp(chargingProfilePurposeStr, "TxDefaultProfile")) {
        chargingProfilePurpose = ChargingProfilePurposeType::TxDefaultProfile;
    //} else if (!strcmp(chargingProfilePurposeStr, "TxProfile")) {
    } else {
        chargingProfilePurpose = ChargingProfilePurposeType::TxProfile;
    }
    const char *chargingProfileKindStr = json["chargingProfileKind"] | "Invalid";
    if (!strcmp(chargingProfileKindStr, "Absolute")) {
        chargingProfileKind = ChargingProfileKindType::Absolute;
    } else if (!strcmp(chargingProfileKindStr, "Recurring")) {
        chargingProfileKind = ChargingProfileKindType::Recurring;
    //} else if (!strcmp(chargingProfileKindStr, "Relative")) {
    } else {
        chargingProfileKind = ChargingProfileKindType::Relative;
    }
    const char *recurrencyKindStr = json["recurrencyKind"] | "Invalid";
    if (!strcmp(recurrencyKindStr, "Daily")) {
        recurrencyKind = RecurrencyKindType::Daily;
    } else if (!strcmp(recurrencyKindStr, "Weekly")) {
        recurrencyKind = RecurrencyKindType::Weekly;
    } else {
        recurrencyKind = RecurrencyKindType::NOT_SET; //not part of OCPP 1.6
    }

    AO_DBG_DEBUG("Deserialize JSON: chargingProfileId=%i, chargingProfilePurpose=%s, recurrencyKind=%s", chargingProfileId, chargingProfilePurposeStr, recurrencyKindStr);

    if (!validFrom.setTime(json["validFrom"] | "Invalid")) {
        //non-success
        AO_DBG_ERR("validFrom format violation. Expect format like 2022-02-01T20:53:32.486Z. Assume unlimited validity");
        validFrom = MIN_TIME;
    }

    if (!validTo.setTime(json["validTo"] | "Invalid")) {
        //non-success
        AO_DBG_ERR("validTo format violation. Expect format like 2022-02-01T20:53:32.486Z. Assume unlimited validity");
        validTo = MIN_TIME;
    }

    JsonObject schedule = json["chargingSchedule"]; 
    chargingSchedule = std::unique_ptr<ChargingSchedule>(new ChargingSchedule(schedule, chargingProfileKind, recurrencyKind));
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

    char tmp[JSONDATE_LENGTH + 1] = {'\0'};
    validFrom.toJsonString(tmp, JSONDATE_LENGTH + 1);
    char tmp2[JSONDATE_LENGTH + 1] = {'\0'};
    validTo.toJsonString(tmp2, JSONDATE_LENGTH + 1);

    AO_DBG_INFO("CHARGING PROFILE:\n" \
                "  chargingProfileId: %i\n" \
                "  transactionId: %i\n" \
                "  stackLevel: %i\n" \
                "  chargingProfilePurpose: %s\n" \
                "  chargingProfileKind: %s\n" \
                "  recurrencyKind: %s\n" \
                "  validFrom: %s\n" \
                "  validTo: %s\n",
                chargingProfileId,
                transactionId,
                stackLevel,
                chargingProfilePurpose == (ChargingProfilePurposeType::ChargePointMaxProfile) ? "ChargePointMaxProfile" :
                    chargingProfilePurpose == (ChargingProfilePurposeType::TxDefaultProfile) ? "TxDefaultProfile" :
                    chargingProfilePurpose == (ChargingProfilePurposeType::TxProfile) ? "TxProfile" : "Error",
                chargingProfileKind == (ChargingProfileKindType::Absolute) ? "Absolute" :
                    chargingProfileKind == (ChargingProfileKindType::Recurring) ? "Recurring" :
                    chargingProfileKind == (ChargingProfileKindType::Relative) ? "Relative" : "Error",
                recurrencyKind == (RecurrencyKindType::Daily) ? "Daily" :
                    recurrencyKind == (RecurrencyKindType::Weekly) ? "Weekly" :
                    recurrencyKind == (RecurrencyKindType::NOT_SET) ? "NOT_SET" : "Error",
                tmp,
                tmp2
                );

    chargingSchedule->printSchedule();
}
