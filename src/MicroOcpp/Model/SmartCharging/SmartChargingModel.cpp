// matth-x/MicroOcpp
// Copyright Matthias Akstaller 2019 - 2023
// MIT License

#include <MicroOcpp/Model/SmartCharging/SmartChargingModel.h>
#include <MicroOcpp/Debug.h>

#include <string.h>
#include <algorithm>

using namespace MicroOcpp;

ChargeRate MicroOcpp::chargeRate_min(const ChargeRate& a, const ChargeRate& b) {
    ChargeRate res;
    res.power = std::min(a.power, b.power);
    res.current = std::min(a.current, b.current);
    res.nphases = std::min(a.nphases, b.nphases);
    return res;
}

bool ChargingSchedule::calculateLimit(const Timestamp &t, const Timestamp &startOfCharging, ChargeRate& limit, Timestamp& nextChange) {
    Timestamp basis = Timestamp(); //point in time to which schedule-related times are relative
    switch (chargingProfileKind) {
        case (ChargingProfileKindType::Absolute):
            //check if schedule is not valid yet but begins in future
            if (startSchedule > t) {
                //not valid YET
                nextChange = std::min(nextChange, startSchedule);
                return false;
            }
            //If charging profile is absolute, prefer startSchedule as basis. If absent, use chargingStart instead. If absent, no
            //behaviour is defined
            if (startSchedule > MIN_TIME) {
                basis = startSchedule;
            } else if (startOfCharging > MIN_TIME && startOfCharging < t) {
                basis = startOfCharging;
            } else {
                MO_DBG_ERR("Absolute profile, but neither startSchedule, nor start of charging are set. Undefined behavior, abort");
                return false;
            }
            break;
        case (ChargingProfileKindType::Recurring):
            if (recurrencyKind == RecurrencyKindType::Daily) {
                basis = t - ((t - startSchedule) % (24 * 3600));
                nextChange = std::min(nextChange, basis + (24 * 3600)); //constrain nextChange to basis + one day
            } else if (recurrencyKind == RecurrencyKindType::Weekly) {
                basis = t - ((t - startSchedule) % (7 * 24 * 3600));
                nextChange = std::min(nextChange, basis + (7 * 24 * 3600));
            } else {
                MO_DBG_ERR("Recurring ChargingProfile but no RecurrencyKindType set. Undefined behavior, assume 'Daily'");
                basis = t - ((t - startSchedule) % (24 * 3600));
                nextChange = std::min(nextChange, basis + (24 * 3600));
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
        MO_DBG_ERR("time basis is smaller than t, but t must be >= basis");
        return false;
    }
    
    int t_toBasis = t - basis;

    if (duration > 0){
        //duration is set

        //check if duration is exceeded and if yes, abort limit algorithm
        //if no, the duration is an upper limit for the validity of the schedule
        if (t_toBasis >= duration) { //"duration" is given relative to basis
            return false;
        } else {
            nextChange = std::min(nextChange, basis + duration);
        }
    }

    /*
    * Work through the ChargingProfilePeriods here. If the right period was found, assign the limit parameter from it
    * and make nextChange equal the beginning of the following period. If the right period is the last one, nextChange
    * will remain the time determined before.
    */
    float limit_res = -1.0f; //If limit_res is still -1 after the loop, the limit algorithm failed
    int nphases_res = -1;
    for (auto period = chargingSchedulePeriod.begin(); period != chargingSchedulePeriod.end(); period++) {
        if (period->startPeriod > t_toBasis) {
            // found the first period that comes after t_toBasis.
            nextChange = basis + period->startPeriod;
            nextChange = std::min(nextChange, basis + period->startPeriod);
            break; //The currently valid limit was set the iteration before
        }
        limit_res = period->limit;
        nphases_res = period->numberPhases;
    }
    
    if (limit_res >= 0.0f) {
        limit_res = std::max(limit_res, minChargingRate);

        if (chargingRateUnit == ChargingRateUnitType::Amp) {
            limit.current = limit_res;
        } else {
            limit.power = limit_res;
        }

        limit.nphases = nphases_res;
        return true;
    } else {
        return false; //No limit was found. Either there is no ChargingProfilePeriod, or each period begins after t_toBasis
    }
}

bool ChargingSchedule::toJson(DynamicJsonDocument& doc) {
    size_t capacity = 0;
    capacity += JSON_OBJECT_SIZE(5); //no of fields of ChargingSchedule
    capacity += JSONDATE_LENGTH + 1; //startSchedule
    capacity += JSON_ARRAY_SIZE(chargingSchedulePeriod.size()) + chargingSchedulePeriod.size() * JSON_OBJECT_SIZE(3);

    doc = DynamicJsonDocument(capacity);
    if (duration >= 0) {
        doc["duration"] = duration;
    }
    char startScheduleJson [JSONDATE_LENGTH + 1] = {'\0'};
    startSchedule.toJsonString(startScheduleJson, JSONDATE_LENGTH + 1);
    doc["startSchedule"] = startScheduleJson;
    doc["chargingRateUnit"] = chargingRateUnit == (ChargingRateUnitType::Amp) ? "A" : "W";
    JsonArray periodArray = doc.createNestedArray("chargingSchedulePeriod");
    for (auto period = chargingSchedulePeriod.begin(); period != chargingSchedulePeriod.end(); period++) {
        JsonObject entry = periodArray.createNestedObject();
        entry["startPeriod"] = period->startPeriod;
        entry["limit"] = period->limit;
        if (period->numberPhases != 3) {
            entry["numberPhases"] = period->numberPhases;
        }
    }
    if (minChargingRate >= 0) {
        doc["minChargeRate"] = minChargingRate;
    }

    return true;
}

void ChargingSchedule::printSchedule(){

    char tmp[JSONDATE_LENGTH + 1] = {'\0'};
    startSchedule.toJsonString(tmp, JSONDATE_LENGTH + 1);

    MO_CONSOLE_PRINTF("   > CHARGING SCHEDULE:\n" \
                "       > duration: %i\n" \
                "       > startSchedule: %s\n" \
                "       > chargingRateUnit: %s\n" \
                "       > minChargingRate: %f\n",
                duration,
                tmp,
                chargingRateUnit == (ChargingRateUnitType::Amp) ? "A" :
                    chargingRateUnit == (ChargingRateUnitType::Watt) ? "W" : "Error",
                minChargingRate);

    for (auto period = chargingSchedulePeriod.begin(); period != chargingSchedulePeriod.end(); period++) {
        MO_CONSOLE_PRINTF("       > CHARGING SCHEDULE PERIOD:\n" \
                "           > startPeriod: %i\n" \
                "           > limit: %f\n" \
                "           > numberPhases: %i\n",
                period->startPeriod,
                period->limit,
                period->numberPhases);
    }
}

bool ChargingProfile::calculateLimit(const Timestamp &t, const Timestamp &startOfCharging, ChargeRate& limit, Timestamp& nextChange){
    if (t > validTo && validTo > MIN_TIME) {
        return false; //no limit defined
    }
    if (t < validFrom) {
        nextChange = std::min(nextChange, validFrom);
        return false; //no limit defined
    }

    return chargingSchedule.calculateLimit(t, startOfCharging, limit, nextChange);
}

bool ChargingProfile::calculateLimit(const Timestamp &t, ChargeRate& limit, Timestamp& nextChange){
    return calculateLimit(t, MIN_TIME, limit, nextChange);
}

int ChargingProfile::getChargingProfileId() {
    return chargingProfileId;
}

int ChargingProfile::getTransactionId() {
    return transactionId;
}

int ChargingProfile::getStackLevel(){
    return stackLevel;
}
  
ChargingProfilePurposeType ChargingProfile::getChargingProfilePurpose(){
    return chargingProfilePurpose;
}

bool ChargingProfile::toJson(DynamicJsonDocument& doc) {
    
    DynamicJsonDocument chargingScheduleDoc {0};
    if (!chargingSchedule.toJson(chargingScheduleDoc)) {
        return false;
    }

    doc = DynamicJsonDocument(
            JSON_OBJECT_SIZE(9) + //no. of fields in ChargingProfile
            2 * (JSONDATE_LENGTH + 1) + //validFrom and validTo
            chargingScheduleDoc.memoryUsage()); //nested JSON object

    doc["chargingProfileId"] = chargingProfileId;
    if (transactionId >= 0) {
        doc["transactionId"] = transactionId;
    }
    doc["stackLevel"] = stackLevel;

    switch (chargingProfilePurpose) {
        case (ChargingProfilePurposeType::ChargePointMaxProfile):
            doc["chargingProfilePurpose"] = "ChargePointMaxProfile";
            break;
        case (ChargingProfilePurposeType::TxDefaultProfile):
            doc["chargingProfilePurpose"] = "TxDefaultProfile";
            break;
        case (ChargingProfilePurposeType::TxProfile):
            doc["chargingProfilePurpose"] = "TxProfile";
            break;
    }

    switch (chargingProfileKind) {
        case (ChargingProfileKindType::Absolute):
            doc["chargingProfileKind"] = "Absolute";
            break;
        case (ChargingProfileKindType::Recurring):
            doc["chargingProfileKind"] = "Recurring";
            break;
        case (ChargingProfileKindType::Relative):
            doc["chargingProfileKind"] = "Relative";
            break;
    }

    switch (recurrencyKind) {
        case (RecurrencyKindType::Daily):
            doc["recurrencyKind"] = "Daily";
            break;
        case (RecurrencyKindType::Weekly):
            doc["recurrencyKind"] = "Weekly";
            break;
        default:
            break;
    }

    char timeStr [JSONDATE_LENGTH + 1] = {'\0'};

    if (validFrom > MIN_TIME) {
        if (!validFrom.toJsonString(timeStr, JSONDATE_LENGTH + 1)) {
            MO_DBG_ERR("serialization error");
            return false;
        }
        doc["validFrom"] = timeStr;
    }

    if (validTo > MIN_TIME) {
        if (!validTo.toJsonString(timeStr, JSONDATE_LENGTH + 1)) {
            MO_DBG_ERR("serialization error");
            return false;
        }
        doc["validTo"] = timeStr;
    }

    doc["chargingSchedule"] = chargingScheduleDoc;

    return true;
}

void ChargingProfile::printProfile(){

    char tmp[JSONDATE_LENGTH + 1] = {'\0'};
    validFrom.toJsonString(tmp, JSONDATE_LENGTH + 1);
    char tmp2[JSONDATE_LENGTH + 1] = {'\0'};
    validTo.toJsonString(tmp2, JSONDATE_LENGTH + 1);

    MO_CONSOLE_PRINTF("   > CHARGING PROFILE:\n" \
                "   > chargingProfileId: %i\n" \
                "   > transactionId: %i\n" \
                "   > stackLevel: %i\n" \
                "   > chargingProfilePurpose: %s\n",
                chargingProfileId,
                transactionId,
                stackLevel,
                chargingProfilePurpose == (ChargingProfilePurposeType::ChargePointMaxProfile) ? "ChargePointMaxProfile" :
                    chargingProfilePurpose == (ChargingProfilePurposeType::TxDefaultProfile) ? "TxDefaultProfile" :
                    chargingProfilePurpose == (ChargingProfilePurposeType::TxProfile) ? "TxProfile" : "Error"
                );
    MO_CONSOLE_PRINTF(
                "   > chargingProfileKind: %s\n" \
                "   > recurrencyKind: %s\n" \
                "   > validFrom: %s\n" \
                "   > validTo: %s\n",
                chargingProfileKind == (ChargingProfileKindType::Absolute) ? "Absolute" :
                    chargingProfileKind == (ChargingProfileKindType::Recurring) ? "Recurring" :
                    chargingProfileKind == (ChargingProfileKindType::Relative) ? "Relative" : "Error",
                recurrencyKind == (RecurrencyKindType::Daily) ? "Daily" :
                    recurrencyKind == (RecurrencyKindType::Weekly) ? "Weekly" :
                    recurrencyKind == (RecurrencyKindType::NOT_SET) ? "NOT_SET" : "Error",
                tmp,
                tmp2
                );

    chargingSchedule.printSchedule();
}

namespace MicroOcpp {

bool loadChargingSchedulePeriod(JsonObject& json, ChargingSchedulePeriod& out) {
    int startPeriod = json["startPeriod"] | -1;
    if (startPeriod >= 0) {
        out.startPeriod = startPeriod;
    } else {
        MO_DBG_WARN("format violation");
        return false;
    }

    float limit = json["limit"] | -1.f;
    if (limit >= 0.f) {
        out.limit = limit;
    } else {
        MO_DBG_WARN("format violation");
        return false;
    }

    if (json.containsKey("numberPhases")) {
        int numberPhases = json["numberPhases"];
        if (numberPhases >= 0 && numberPhases <= 3) {
            out.numberPhases = numberPhases;
        } else {
            MO_DBG_WARN("format violation");
            return false;
        }
    }

    return true;
}

} //end namespace MicroOcpp

std::unique_ptr<ChargingProfile> MicroOcpp::loadChargingProfile(JsonObject& json) {
    auto res = std::unique_ptr<ChargingProfile>(new ChargingProfile());

    int chargingProfileId = json["chargingProfileId"] | -1;
    if (chargingProfileId >= 0) {
        res->chargingProfileId = chargingProfileId;
    } else {
        MO_DBG_WARN("format violation");
        return nullptr;
    }

    int transactionId = json["transactionId"] | -1;
    if (transactionId >= 0) {
        res->transactionId = transactionId;
    }

    int stackLevel = json["stackLevel"] | -1;
    if (stackLevel >= 0 && stackLevel <= CHARGEPROFILEMAXSTACKLEVEL) {
        res->stackLevel = stackLevel;
    } else {
        MO_DBG_WARN("format violation");
        return nullptr;
    }

    const char *chargingProfilePurposeStr = json["chargingProfilePurpose"] | "Invalid";
    if (!strcmp(chargingProfilePurposeStr, "ChargePointMaxProfile")) {
        res->chargingProfilePurpose = ChargingProfilePurposeType::ChargePointMaxProfile;
    } else if (!strcmp(chargingProfilePurposeStr, "TxDefaultProfile")) {
        res->chargingProfilePurpose = ChargingProfilePurposeType::TxDefaultProfile;
    } else if (!strcmp(chargingProfilePurposeStr, "TxProfile")) {
        res->chargingProfilePurpose = ChargingProfilePurposeType::TxProfile;
    } else {
        MO_DBG_WARN("format violation");
        return nullptr;
    }

    const char *chargingProfileKindStr = json["chargingProfileKind"] | "Invalid";
    if (!strcmp(chargingProfileKindStr, "Absolute")) {
        res->chargingProfileKind = ChargingProfileKindType::Absolute;
    } else if (!strcmp(chargingProfileKindStr, "Recurring")) {
        res->chargingProfileKind = ChargingProfileKindType::Recurring;
    } else if (!strcmp(chargingProfileKindStr, "Relative")) {
        res->chargingProfileKind = ChargingProfileKindType::Relative;
    } else {
        MO_DBG_WARN("format violation");
        return nullptr;
    }

    const char *recurrencyKindStr = json["recurrencyKind"] | "Invalid";
    if (!strcmp(recurrencyKindStr, "Daily")) {
        res->recurrencyKind = RecurrencyKindType::Daily;
    } else if (!strcmp(recurrencyKindStr, "Weekly")) {
        res->recurrencyKind = RecurrencyKindType::Weekly;
    }

    MO_DBG_DEBUG("Deserialize JSON: chargingProfileId=%i, chargingProfilePurpose=%s, recurrencyKind=%s", chargingProfileId, chargingProfilePurposeStr, recurrencyKindStr);

    if (json.containsKey("validFrom")) {
        if (!res->validFrom.setTime(json["validFrom"] | "Invalid")) {
            //non-success
            MO_DBG_WARN("datetime format violation, expect format like 2022-02-01T20:53:32.486Z");
            return nullptr;
        }
    } else {
        res->validFrom = MIN_TIME;
    }

    if (json.containsKey("validTo")) {
        if (!res->validTo.setTime(json["validTo"] | "Invalid")) {
            //non-success
            MO_DBG_WARN("datetime format violation, expect format like 2022-02-01T20:53:32.486Z");
            return nullptr;
        }
    } else {
        res->validTo = MIN_TIME;
    }

    JsonObject scheduleJson = json["chargingSchedule"];
    ChargingSchedule& schedule = res->chargingSchedule;
    auto success = loadChargingSchedule(scheduleJson, schedule);
    if (!success) {
        return nullptr;
    }

    //duplicate some fields to chargingSchedule to simplify the max charge rate calculation
    schedule.chargingProfileKind = res->chargingProfileKind;
    schedule.recurrencyKind = res->recurrencyKind;

    return res;
}

bool MicroOcpp::loadChargingSchedule(JsonObject& json, ChargingSchedule& out) {
    if (json.containsKey("duration")) {
        int duration = json["duration"] | -1;
        if (duration >= 0) {
            out.duration = duration;
        } else {
            MO_DBG_WARN("format violation");
            return false;
        }
    }

    if (json.containsKey("startSchedule")) {
        if (!out.startSchedule.setTime(json["startSchedule"] | "Invalid")) {
            //non-success
            MO_DBG_WARN("datetime format violation, expect format like 2022-02-01T20:53:32.486Z");
            return false;
        }
    } else {
        out.startSchedule = MIN_TIME;
    }

    const char *unit = json["chargingRateUnit"] | "_Undefined";
    if (unit[0] == 'a' || unit[0] == 'A') {
        out.chargingRateUnit = ChargingRateUnitType::Amp;
    } else if (unit[0] == 'w' || unit[0] == 'W') {
        out.chargingRateUnit = ChargingRateUnitType::Watt;
    } else {
        MO_DBG_WARN("format violation");
        return false;
    }
    
    JsonArray periodJsonArray = json["chargingSchedulePeriod"];
    if (periodJsonArray.size() < 1) {
        MO_DBG_WARN("format violation");
        return false;
    }

    for (JsonObject periodJson : periodJsonArray) {
        out.chargingSchedulePeriod.push_back(ChargingSchedulePeriod());
        if (!loadChargingSchedulePeriod(periodJson, out.chargingSchedulePeriod.back())) {
            return false;
        }
    }
    
    if (json.containsKey("minChargingRate")) {
        float minChargingRate = json["minChargingRate"];
        if (minChargingRate >= 0.f) {
            out.minChargingRate = minChargingRate;
        } else {
            MO_DBG_WARN("format violation");
            return false;
        }
    }

    return true;
}
