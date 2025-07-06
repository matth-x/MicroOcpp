// matth-x/MicroOcpp
// Copyright Matthias Akstaller 2019 - 2024
// MIT License

#include <MicroOcpp/Operations/GetCompositeSchedule.h>

#include <MicroOcpp/Context.h>
#include <MicroOcpp/Model/Model.h>
#include <MicroOcpp/Model/SmartCharging/SmartChargingService.h>
#include <MicroOcpp/Debug.h>

#if (MO_ENABLE_V16 || MO_ENABLE_V201) && MO_ENABLE_SMARTCHARGING

using namespace MicroOcpp;

GetCompositeSchedule::GetCompositeSchedule(Context& context, SmartChargingService& scService) : MemoryManaged("v16.Operation.", "GetCompositeSchedule"), context(context), scService(scService), ocppVersion(context.getOcppVersion()) {

}

const char* GetCompositeSchedule::getOperationType() {
    return "GetCompositeSchedule";
}

void GetCompositeSchedule::processReq(JsonObject payload) {

    #if MO_ENABLE_V16
    if (ocppVersion == MO_OCPP_V16) {
        evseId = payload["connectorId"] | -1;
    }
    #endif //MO_ENABLE_V16
    #if MO_ENABLE_V201
    if (ocppVersion == MO_OCPP_V201) {
        evseId = payload["evseId"] | -1;
    }
    #endif //MO_ENABLE_V201

    duration = payload["duration"] | -1;

    if (evseId < 0 || duration < 0) {
        errorCode = "FormationViolation";
        return;
    }

    if ((unsigned int) evseId >= context.getModelCommon().getNumEvseId()) {
        errorCode = "PropertyConstraintViolation";
    }

    if (payload.containsKey("chargingRateUnit")) {
        chargingRateUnit = deserializeChargingRateUnitType(payload["chargingRateUnit"] | "_Invalid");
        if (chargingRateUnit == ChargingRateUnitType::UNDEFINED) {
            errorCode = "FormationViolation";
            return;
        }
    }
}

std::unique_ptr<JsonDoc> GetCompositeSchedule::createConf(){

    bool success = false;

    auto chargingSchedule = scService.getCompositeSchedule((unsigned int) evseId, duration, chargingRateUnit);
    JsonDoc chargingScheduleDoc (0);

    if (chargingSchedule) {
        auto capacity = chargingSchedule->getJsonCapacity(ocppVersion, /* compositeSchedule */ true);
        chargingScheduleDoc = initJsonDoc(getMemoryTag(), capacity);
        success = chargingSchedule->toJson(context.getClock(), ocppVersion, /* compositeSchedule */ true, chargingScheduleDoc.to<JsonObject>());
    }

    char scheduleStartStr [MO_JSONDATE_SIZE] = {'\0'};

    #if MO_ENABLE_V16
    if (ocppVersion == MO_OCPP_V16) {
        if (success && chargingSchedule) {
            Timestamp scheduleStart;
            if (context.getClock().fromUnixTime(scheduleStart, chargingSchedule->startSchedule)) {
                success = context.getClock().toJsonString(scheduleStart,  scheduleStartStr, sizeof(scheduleStartStr));
            } else {
                success = false;
            }
        }
    }
    #endif //MO_ENABLE_V16

    if (success && chargingSchedule) {
        auto doc = makeJsonDoc(getMemoryTag(),
                        (ocppVersion == MO_OCPP_V16 ?
                            JSON_OBJECT_SIZE(4) + MO_JSONDATE_SIZE :
                            JSON_OBJECT_SIZE(2)) +
                        chargingScheduleDoc.memoryUsage());
        JsonObject payload = doc->to<JsonObject>();
        payload["status"] = "Accepted";
        #if MO_ENABLE_V16
        if (ocppVersion == MO_OCPP_V16) {
            payload["connectorId"] = evseId;
            if (ocppVersion == MO_OCPP_V16) {
                payload["scheduleStart"] = scheduleStartStr;
            }
        }
        #endif //MO_ENABLE_V16
        payload["chargingSchedule"] = chargingScheduleDoc;
        return doc;
    } else {
        auto doc = makeJsonDoc(getMemoryTag(), JSON_OBJECT_SIZE(1));
        JsonObject payload = doc->to<JsonObject>();
        payload["status"] = "Rejected";
        return doc;
    }
}

#endif //(MO_ENABLE_V16 || MO_ENABLE_V201) && MO_ENABLE_SMARTCHARGING
