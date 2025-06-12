// matth-x/MicroOcpp
// Copyright Matthias Akstaller 2019 - 2024
// MIT License

#include <MicroOcpp/Model/SmartCharging/SmartChargingModel.h>
#include <MicroOcpp/Core/Time.h>
#include <MicroOcpp/Debug.h>

#include <string.h>
#include <algorithm>

#if (MO_ENABLE_V16 || MO_ENABLE_V201) && MO_ENABLE_SMARTCHARGING

using namespace MicroOcpp;

void mo_chargeRate_init(MO_ChargeRate *limit) {
    limit->power = -1.f;
    limit->current = -1.f;
    limit->numberPhases = -1;
    #if MO_ENABLE_V201
    limit->phaseToUse = -1;
    #endif
}

bool mo_chargeRate_equals(const MO_ChargeRate *a, const MO_ChargeRate *b) {
    return
        ((a->power < 0 && b->power < 0) || a->power == b->power) &&
        ((a->current < 0 && b->current < 0) || a->current == b->current) &&
        ((a->numberPhases < 0 && b->numberPhases < 0) || a->numberPhases == b->numberPhases)
        #if MO_ENABLE_V201
        && ((a->phaseToUse < 0 && b->phaseToUse < 0) || a->phaseToUse == b->phaseToUse)
        #endif
        ;
}

const char *MicroOcpp::serializeChargingProfilePurposeType(ChargingProfilePurposeType v) {
    const char *res = "";
    switch (v) {
        case ChargingProfilePurposeType::UNDEFINED:
            MO_DBG_WARN("serialization error");
            break;
        case ChargingProfilePurposeType::ChargePointMaxProfile:
            res = "ChargePointMaxProfile";
            break;
        case ChargingProfilePurposeType::TxDefaultProfile:
            res = "TxDefaultProfile";
            break;
        case ChargingProfilePurposeType::TxProfile:
            res = "TxProfile";
            break;
    }
    return res;
}

ChargingProfilePurposeType MicroOcpp::deserializeChargingProfilePurposeType(const char *str) {
    ChargingProfilePurposeType res = ChargingProfilePurposeType::UNDEFINED;
    if (!strcmp(str, "ChargePointMaxProfile")) {
        res = ChargingProfilePurposeType::ChargePointMaxProfile;
    } else if (!strcmp(str, "TxDefaultProfile")) {
        res = ChargingProfilePurposeType::TxDefaultProfile;
    } else if (!strcmp(str, "TxProfile")) {
        res = ChargingProfilePurposeType::TxProfile;
    }
    return res;
}

const char *MicroOcpp::serializeChargingRateUnitType(ChargingRateUnitType v) {
    const char *res = "";
    switch (v) {
        case ChargingRateUnitType::UNDEFINED:
            res = "";
            break;
        case ChargingRateUnitType::Watt:
            res = "W";
            break;
        case ChargingRateUnitType::Amp:
            res = "A";
            break;
    }
    return res;
}

ChargingRateUnitType MicroOcpp::deserializeChargingRateUnitType(const char *str) {
    ChargingRateUnitType res = ChargingRateUnitType::UNDEFINED;
    if (!strcmp(str, "W")) {
        res = ChargingRateUnitType::Watt;
    } else if (!strcmp(str, "A")) {
        res = ChargingRateUnitType::Amp;
    } else {
        MO_DBG_ERR("deserialization error");
    }
    return res;
}

ChargingSchedule::ChargingSchedule() : MemoryManaged("v16.SmartCharging.SmartChargingModel") {

}

ChargingSchedule::~ChargingSchedule() {
    resizeChargingSchedulePeriod(0); // frees all resources if called with 0
}

bool ChargingSchedule::resizeChargingSchedulePeriod(size_t chargingSchedulePeriodSize) {
    for (size_t i = 0; i < this->chargingSchedulePeriodSize; i++) {
        delete chargingSchedulePeriod[i];
        chargingSchedulePeriod[i] = nullptr;
    }
    MO_FREE(chargingSchedulePeriod);
    chargingSchedulePeriod = nullptr;
    this->chargingSchedulePeriodSize = 0;

    if (chargingSchedulePeriodSize == 0) {
        return true;
    }

    chargingSchedulePeriod = static_cast<ChargingSchedulePeriod**>(MO_MALLOC(getMemoryTag("v16.SmartCharging.SmartChargingModel"), sizeof(ChargingSchedulePeriod*) * chargingSchedulePeriodSize));
    if (!chargingSchedulePeriod) {
        MO_DBG_ERR("OOM");
        return false;
    }
    memset(chargingSchedulePeriod, 0, sizeof(ChargingSchedulePeriod*) * chargingSchedulePeriodSize);
    for (size_t i = 0; i < chargingSchedulePeriodSize; i++) {
        chargingSchedulePeriod[i] = new ChargingSchedulePeriod();
        if (!chargingSchedulePeriod[i]) {
            MO_DBG_ERR("OOM");
            resizeChargingSchedulePeriod(0);
            return false;
        }
        this->chargingSchedulePeriodSize = i;
    }

    this->chargingSchedulePeriodSize = chargingSchedulePeriodSize;
    return true;
}

bool ChargingSchedule::calculateLimit(int32_t unixTime, int32_t sessionDurationSecs, MO_ChargeRate& limit, int32_t& nextChangeSecs) {
    if (unixTime == 0 && sessionDurationSecs < 0) {
        MO_DBG_WARN("Neither absolute time nor session duration known. Cannot determine limit");
        return false;
    }
    if (startSchedule == 0 && sessionDurationSecs < 0) { //both undefined
        MO_DBG_WARN("startSchedule not set and session duration unkown. Cannot determine limit");
        return false;
    }

    int32_t scheduleSecs = -1; //time passed since begin of this schedule
    switch (chargingProfileKind) {
        case ChargingProfileKindType::Absolute:
            if (unixTime == 0) {
                //cannot use Absolute profiles when 
                MO_DBG_WARN("Need to set clock before using absolute profiles");
                return false;
            }
            //check if schedule is not valid yet but begins in future
            if (startSchedule != 0 && startSchedule > unixTime) {
                //not valid YET
                nextChangeSecs = std::min(nextChangeSecs, startSchedule - unixTime);
                return false;
            }
            //If charging profile is absolute, prefer startSchedule as basis. If absent, use session duration instead
            if (startSchedule != 0) {
                scheduleSecs = unixTime - startSchedule;
            } else {
                scheduleSecs = sessionDurationSecs; //sessionDurationSecs valid per precondition
            }
            break;
        case ChargingProfileKindType::Recurring: {
            int32_t recurrencySecs = 0;
            switch (recurrencyKind) {
                case RecurrencyKindType::Daily:
                    recurrencySecs = 24 * 3600;
                    break;
                case RecurrencyKindType::Weekly:
                    recurrencySecs = 7 * 24 * 3600;
                    break;
                case RecurrencyKindType::UNDEFINED:
                    MO_DBG_ERR("Recurring ChargingProfile but no RecurrencyKindType set. Assume 'Daily'");
                    recurrencySecs = 24 * 3600;
                    break;
            }
            if (unixTime != 0 && startSchedule != 0) {
                scheduleSecs = (unixTime - startSchedule) % recurrencySecs;
            } else {
                scheduleSecs = sessionDurationSecs % recurrencySecs; //sessionDurationSecs valid per precondition
            }
            nextChangeSecs = std::min(nextChangeSecs, recurrencySecs - scheduleSecs); //constrain nextChange to basis + one day
            break;
        }
        case ChargingProfileKindType::Relative:
            //assumed, that it is relative to start of charging
            //start of charging must be before t or equal to t
            if (sessionDurationSecs < 0) {
                //Relative charging profiles only work with a currently active charging session which is not the case here
                return false;
            }
            scheduleSecs = sessionDurationSecs;
            break;
    }

    if (duration > 0){
        //duration is set

        //check if duration is exceeded and if yes, abort limit algorithm
        //if no, the duration is an upper limit for the validity of the schedule
        if (scheduleSecs >= duration) { //"duration" is given relative to basis
            return false;
        } else {
            nextChangeSecs = std::min(nextChangeSecs, duration - scheduleSecs);
        }
    }

    /*
    * Work through the ChargingProfilePeriods here. If the right period was found, assign the limit parameter from it
    * and make nextChange equal the beginning of the following chargingSchedulePeriod[i]. If the right period is the last one, nextChange
    * will remain the time determined before.
    */
    float limit_res = -1.0f; //If limit_res is still -1 after the loop, the limit algorithm failed
    int numberPhases_res = -1;
    for (size_t i = 0; i < chargingSchedulePeriodSize; i++) {
        if (chargingSchedulePeriod[i]->startPeriod > scheduleSecs) {
            // found the first period that comes after t_toBasis.
            nextChangeSecs = std::min(nextChangeSecs, chargingSchedulePeriod[i]->startPeriod - scheduleSecs);
            break; //The currently valid limit was set the iteration before
        }
        limit_res = chargingSchedulePeriod[i]->limit;
        numberPhases_res = chargingSchedulePeriod[i]->numberPhases;
    }

    if (limit_res >= 0.0f) {
        limit_res = std::max(limit_res, minChargingRate);

        if (chargingRateUnit == ChargingRateUnitType::Amp) {
            limit.current = limit_res;
        } else {
            limit.power = limit_res;
        }

        limit.numberPhases = numberPhases_res;
        return true;
    } else {
        return false; //No limit was found. Either there is no ChargingProfilePeriod, or each period begins after t_toBasis
    }
}

bool ChargingSchedule::parseJson(Clock& clock, int ocppVersion, JsonObject json) {

    #if MO_ENABLE_V201
    if (ocppVersion == MO_OCPP_V201) {
        id = json["id"] | -1;
        if (id < 0) {
            MO_DBG_WARN("format violation");
            return false;
        }
    }
    #endif //MO_ENABLE_V201

    if (json.containsKey("duration")) {
        duration = json["duration"] | -1;
        if (duration < 0) {
            MO_DBG_WARN("format violation");
            return false;
        }
    }

    if (json.containsKey("startSchedule")) {
        Timestamp startSchedule2;
        if (!clock.parseString(json["startSchedule"] | "_Invalid", startSchedule2)) {
            //non-success
            MO_DBG_WARN("datetime format violation, expect format like 2022-02-01T20:53:32.486Z");
            return false;
        }
        if (!clock.toUnixTime(startSchedule2, startSchedule)) {
            MO_DBG_ERR("internal error");
            return false;
        }
    }

    chargingRateUnit = deserializeChargingRateUnitType(json["chargingRateUnit"] | "_Undefined");
    if (chargingRateUnit == ChargingRateUnitType::UNDEFINED) {
        MO_DBG_WARN("format violation");
        return false;
    }

    JsonArray periodJsonArray = json["chargingSchedulePeriod"];
    if (periodJsonArray.size() < 1) {
        MO_DBG_WARN("format violation");
        return false;
    }

    size_t chargingSchedulePeriodSize = periodJsonArray.size();

    if (chargingSchedulePeriodSize > MO_ChargingScheduleMaxPeriods) {
        MO_DBG_WARN("exceed ChargingScheduleMaxPeriods");
        return false;
    }

    if (!resizeChargingSchedulePeriod(chargingSchedulePeriodSize)) {
        MO_DBG_ERR("OOM");
        return false;
    }

    for (size_t i = 0; i < chargingSchedulePeriodSize; i++) {

        JsonObject periodJson = periodJsonArray[i];
        auto& period = *chargingSchedulePeriod[i];

        period.startPeriod = periodJson["startPeriod"] | -1;
        if (period.startPeriod < 0) {
            MO_DBG_WARN("format violation");
            return false;
        }

        period.limit = json["limit"] | -1.f;
        if (period.limit < 0.f) {
            MO_DBG_WARN("format violation");
            return false;
        }

        if (json.containsKey("numberPhases")) {
            period.numberPhases = json["numberPhases"] | -1;
            if (period.numberPhases < 0 || period.numberPhases > 3) {
                MO_DBG_WARN("format violation");
                return false;
            }
        }

        #if MO_ENABLE_V201
        if (ocppVersion == MO_OCPP_V201) {
            if (json.containsKey("phaseToUse")) {
                period.phaseToUse = json["phaseToUse"] | -1;
                if (period.phaseToUse < 0 || period.phaseToUse > 3) {
                    MO_DBG_WARN("format violation");
                    return false;
                }
            }
        }
        #endif //MO_ENABLE_V201
    }

    if (json.containsKey("minChargingRate")) {
        float minChargingRateIn = json["minChargingRate"];
        if (minChargingRateIn >= 0.f) {
            minChargingRate = minChargingRateIn;
        } else {
            MO_DBG_WARN("format violation");
            return false;
        }
    }

    return true;
}

size_t ChargingSchedule::getJsonCapacity(int ocppVersion, bool compositeSchedule) {
    size_t capacity = 0;

    #if MO_ENABLE_V16
    if (ocppVersion == MO_OCPP_V16) {
        capacity += JSON_OBJECT_SIZE(5); //no of fields of ChargingSchedule
    }
    #endif //MO_ENABLE_V16
    #if MO_ENABLE_V201
    if (ocppVersion == MO_OCPP_V201) {
        if (compositeSchedule) {
            capacity += JSON_OBJECT_SIZE(5); //no of fields of a compositie ChargingSchedule
        } else {
            capacity += JSON_OBJECT_SIZE(6); //no of fields of ChargingSchedule
        }
    }
    #endif //MO_ENABLE_V201

    capacity += MO_JSONDATE_SIZE; //startSchedule / scheduleStart
    capacity += JSON_ARRAY_SIZE(chargingSchedulePeriodSize) + chargingSchedulePeriodSize * (
        ocppVersion == MO_OCPP_V16 ?
            JSON_OBJECT_SIZE(3) :
            JSON_OBJECT_SIZE(4)
        );

    return capacity;
}

bool ChargingSchedule::toJson(Clock& clock, int ocppVersion, bool compositeSchedule, JsonObject out, int evseId) {

    char startScheduleStr [MO_JSONDATE_SIZE] = {'\0'};
    if (startSchedule) {
        Timestamp startSchedule2;
        if (!clock.fromUnixTime(startSchedule2, startSchedule)) {
            MO_DBG_ERR("serialization error");
            return false;
        }
        char startScheduleStr [MO_JSONDATE_SIZE] = {'\0'};
        if (!clock.toJsonString(startSchedule2, startScheduleStr, sizeof(startScheduleStr))) {
            MO_DBG_ERR("serialization error");
            return false;
        }
    }

    #if MO_ENABLE_V16
    if (ocppVersion == MO_OCPP_V16) {
        out["startSchedule"] = startScheduleStr;
        if (minChargingRate >= 0) {
            out["minChargeRate"] = minChargingRate;
        }
    }
    #endif //MO_ENABLE_V16
    #if MO_ENABLE_V201
    if (ocppVersion == MO_OCPP_V201) {
        if (compositeSchedule) {
            out["evseId"] = evseId;
            out["scheduleStart"] = startScheduleStr;
        } else {
            out["id"] = id;
            out["startSchedule"] = startScheduleStr;
            if (minChargingRate >= 0) {
                out["minChargeRate"] = minChargingRate;
            }
            //salesTariff not supported
        }
    }
    #endif //MO_ENABLE_V201

    if (duration >= 0) {
        out["duration"] = duration;
    }
    out["chargingRateUnit"] = serializeChargingRateUnitType(chargingRateUnit);

    JsonArray periodArray = out.createNestedArray("chargingSchedulePeriod");
    for (size_t i = 0; i < chargingSchedulePeriodSize; i++) {
        JsonObject entry = periodArray.createNestedObject();
        entry["startPeriod"] = chargingSchedulePeriod[i]->startPeriod;
        entry["limit"] = chargingSchedulePeriod[i]->limit;
        if (chargingSchedulePeriod[i]->numberPhases != 3) {
            entry["numberPhases"] = chargingSchedulePeriod[i]->numberPhases;
        }
        #if MO_ENABLE_V201
        if (ocppVersion == MO_OCPP_V201) {
            if (chargingSchedulePeriod[i]->phaseToUse >= 0) {
                entry["numberPhases"] = chargingSchedulePeriod[i]->numberPhases;
            }
        }
        #endif //MO_ENABLE_V201
    }

    return true;
}

void ChargingSchedule::printSchedule(Clock& clock){

    char tmp[MO_JSONDATE_SIZE] = {'\0'};
    if (startSchedule) {
        Timestamp startSchedule2;
        clock.fromUnixTime(startSchedule2, startSchedule);
        clock.toJsonString(startSchedule2, tmp, sizeof(tmp));
    }

    MO_DBG_VERBOSE("   > CHARGING SCHEDULE:\n" \
                "       > duration: %i\n" \
                "       > startSchedule: %s\n" \
                "       > chargingRateUnit: %s\n" \
                "       > minChargingRate: %f\n",
                duration,
                tmp,
                serializeChargingRateUnitType(chargingRateUnit),
                minChargingRate);

    for (size_t i = 0; i < chargingSchedulePeriodSize; i++) {
        MO_DBG_VERBOSE("       > CHARGING SCHEDULE PERIOD:\n" \
                "           > startPeriod: %i\n" \
                "           > limit: %f\n" \
                "           > numberPhases: %i\n",
                chargingSchedulePeriod[i]->startPeriod,
                chargingSchedulePeriod[i]->limit,
                chargingSchedulePeriod[i]->numberPhases);
    }
}

ChargingProfile::ChargingProfile() : MemoryManaged("v16.SmartCharging.ChargingProfile") {

}

bool ChargingProfile::calculateLimit(int32_t unixTime, int32_t sessionDurationSecs, MO_ChargeRate& limit, int32_t& nextChangeSecs){
    if (unixTime && validTo && unixTime > validTo) {
        return false; //no limit defined
    }
    if (unixTime && validFrom && unixTime < validFrom) {
        nextChangeSecs = std::min(nextChangeSecs, validFrom - unixTime);
        return false; //no limit defined
    }

    return chargingSchedule.calculateLimit(unixTime, sessionDurationSecs, limit, nextChangeSecs);
}

bool ChargingProfile::parseJson(Clock& clock, int ocppVersion, JsonObject json) {

    chargingProfileId = json["chargingProfileId"] | -1;
    if (chargingProfileId < 0) {
        MO_DBG_WARN("format violation");
        return false;
    }

    #if MO_ENABLE_V16
    if (ocppVersion == MO_OCPP_V16) {
        int transactionId = json["transactionId"] | -1;
        if (transactionId >= 0) {
            transactionId16 = transactionId;
        }
    }
    #endif //MO_ENABLE_V16
    #if MO_ENABLE_V201
    if (ocppVersion == MO_OCPP_V201) {
        if (json.containsKey("transactionId")) {
            auto ret = snprintf(transactionId201, sizeof(transactionId201), "%s", json["transactionId"] | "");
            if (ret < 0 || ret >= sizeof(transactionId201)) {
                MO_DBG_WARN("format violation");
                return false;
            }
        }
    }
    #endif //MO_ENABLE_V201

    stackLevel = json["stackLevel"] | -1;
    if (stackLevel < 0 || stackLevel > MO_ChargeProfileMaxStackLevel) {
        MO_DBG_WARN("format violation");
        return false;
    }

    chargingProfilePurpose = deserializeChargingProfilePurposeType(json["chargingProfilePurpose"] | "_Invalid");
    if (chargingProfilePurpose == ChargingProfilePurposeType::UNDEFINED) {
        MO_DBG_WARN("format violation");
        return false;
    }

    const char *chargingProfileKindStr = json["chargingProfileKind"] | "_Invalid";
    if (!strcmp(chargingProfileKindStr, "Absolute")) {
        chargingProfileKind = ChargingProfileKindType::Absolute;
    } else if (!strcmp(chargingProfileKindStr, "Recurring")) {
        chargingProfileKind = ChargingProfileKindType::Recurring;
    } else if (!strcmp(chargingProfileKindStr, "Relative")) {
        chargingProfileKind = ChargingProfileKindType::Relative;
    } else {
        MO_DBG_WARN("format violation");
        return false;
    }

    if (json.containsKey("recurrencyKind")) {
        const char *recurrencyKindStr = json["recurrencyKind"] | "_Invalid";
        if (!strcmp(recurrencyKindStr, "Daily")) {
            recurrencyKind = RecurrencyKindType::Daily;
        } else if (!strcmp(recurrencyKindStr, "Weekly")) {
            recurrencyKind = RecurrencyKindType::Weekly;
        } else {
            MO_DBG_WARN("format violation");
            return false;
        }
    }

    MO_DBG_DEBUG("Deserialize JSON: chargingProfileId=%i, chargingProfilePurpose=%s", chargingProfileId, serializeChargingProfilePurposeType(chargingProfilePurpose));

    if (json.containsKey("validFrom")) {
        Timestamp validFrom2;
        if (!clock.parseString(json["validFrom"] | "_Invalid", validFrom2)) {
            //non-success
            MO_DBG_WARN("datetime format violation, expect format like 2022-02-01T20:53:32.486Z");
            return false;
        }
        if (!clock.toUnixTime(validFrom2, validFrom)) {
            MO_DBG_ERR("internal error");
            return false;
        }
    }

    if (json.containsKey("validTo")) {
        Timestamp validTo2;
        if (!clock.parseString(json["validTo"] | "_Invalid", validTo2)) {
            //non-success
            MO_DBG_WARN("datetime format violation, expect format like 2022-02-01T20:53:32.486Z");
            return false;
        }
        if (!clock.toUnixTime(validTo2, validTo)) {
            MO_DBG_ERR("internal error");
            return false;
        }
    }

    auto success = chargingSchedule.parseJson(clock, ocppVersion, json["chargingSchedule"]);
    if (!success) {
        return false;
    }

    //duplicate some fields to chargingSchedule to simplify the max charge rate calculation
    chargingSchedule.chargingProfileKind = chargingProfileKind;
    chargingSchedule.recurrencyKind = recurrencyKind;

    return true;
}

size_t ChargingProfile::getJsonCapacity(int ocppVersion) {
    size_t capacity = 0;

    capacity += chargingSchedule.getJsonCapacity(ocppVersion, /*compositeSchedule*/ false);

    capacity += JSON_OBJECT_SIZE(9) + //no. of fields in ChargingProfile
                2 * MO_JSONDATE_SIZE; //validFrom and validTo

    return capacity;
}

bool ChargingProfile::toJson(Clock& clock, int ocppVersion, JsonObject out) {

    out["chargingProfileId"] = chargingProfileId;

    #if MO_ENABLE_V16
    if (ocppVersion == MO_OCPP_V16) {
        if (transactionId16 >= 0) {
            out["transactionId"] = transactionId16;
        }
    }
    #endif //MO_ENABLE_V16
    #if MO_ENABLE_V201
    if (ocppVersion == MO_OCPP_V201) {
        if (*transactionId201) {
            out["transactionId"] = (const char*)transactionId201; //force zero-copy
        }
    }
    #endif //MO_ENABLE_V201

    out["stackLevel"] = stackLevel;

    out["chargingProfilePurpose"] = serializeChargingProfilePurposeType(chargingProfilePurpose);

    switch (chargingProfileKind) {
        case ChargingProfileKindType::Absolute:
            out["chargingProfileKind"] = "Absolute";
            break;
        case ChargingProfileKindType::Recurring:
            out["chargingProfileKind"] = "Recurring";
            break;
        case ChargingProfileKindType::Relative:
            out["chargingProfileKind"] = "Relative";
            break;
    }

    switch (recurrencyKind) {
        case RecurrencyKindType::Daily:
            out["recurrencyKind"] = "Daily";
            break;
        case RecurrencyKindType::Weekly:
            out["recurrencyKind"] = "Weekly";
            break;
        default:
            break;
    }

    if (validFrom) {
        Timestamp validFrom2;
        if (!clock.fromUnixTime(validFrom2, validFrom)) {
            MO_DBG_ERR("serialization error");
            return false;
        }
        char validFromStr [MO_JSONDATE_SIZE] = {'\0'};
        if (!clock.toJsonString(validFrom2, validFromStr, sizeof(validFromStr))) {
            MO_DBG_ERR("serialization error");
            return false;
        }
        out["validFrom"] = validFromStr;
    }

    if (validTo) {
        Timestamp validTo2;
        if (!clock.fromUnixTime(validTo2, validTo)) {
            MO_DBG_ERR("serialization error");
            return false;
        }
        char validToStr [MO_JSONDATE_SIZE] = {'\0'};
        if (!clock.toJsonString(validTo2, validToStr, sizeof(validToStr))) {
            MO_DBG_ERR("serialization error");
            return false;
        }
        out["validTo"] = validToStr;
    }

    JsonObject chargingScheduleJson = out.createNestedObject("chargingSchedule");
    if (!chargingSchedule.toJson(clock, ocppVersion, /*compositeSchedule*/ false, chargingScheduleJson)) {
        return false;
    }

    return true;
}

void ChargingProfile::printProfile(Clock& clock){

    char validFromStr [MO_JSONDATE_SIZE] = {'\0'};
    if (validFrom) {
        Timestamp validFrom2;
        clock.fromUnixTime(validFrom2, validFrom);
        clock.toJsonString(validFrom2, validFromStr, sizeof(validFromStr));
    }

    char validToStr [MO_JSONDATE_SIZE] = {'\0'};
    if (validTo) {
        Timestamp validTo2;
        clock.fromUnixTime(validTo2, validTo);
        clock.toJsonString(validTo2, validToStr, sizeof(validToStr));
    }

    MO_DBG_VERBOSE("   > CHARGING PROFILE:\n" \
                "   > chargingProfileId: %i\n" \
                "   > transactionId16: %i\n" \
                "   > transactionId201: %i\n" \
                "   > stackLevel: %i\n" \
                "   > chargingProfilePurpose: %s\n",
                chargingProfileId,
                transactionId16,
                transactionId201,
                stackLevel,
                serializeChargingProfilePurposeType(chargingProfilePurpose)
                );
    MO_DBG_VERBOSE(
                "   > chargingProfileKind: %s\n" \
                "   > recurrencyKind: %s\n" \
                "   > validFrom: %s\n" \
                "   > validTo: %s\n",
                chargingProfileKind == (ChargingProfileKindType::Absolute) ? "Absolute" :
                    chargingProfileKind == (ChargingProfileKindType::Recurring) ? "Recurring" :
                    chargingProfileKind == (ChargingProfileKindType::Relative) ? "Relative" : "Error",
                recurrencyKind == (RecurrencyKindType::Daily) ? "Daily" :
                    recurrencyKind == (RecurrencyKindType::Weekly) ? "Weekly" :
                    recurrencyKind == (RecurrencyKindType::UNDEFINED) ? "NOT_SET" : "Error",
                validFrom,
                validTo
                );

    chargingSchedule.printSchedule(clock);
}

#endif //(MO_ENABLE_V16 || MO_ENABLE_V201) && MO_ENABLE_SMARTCHARGING
