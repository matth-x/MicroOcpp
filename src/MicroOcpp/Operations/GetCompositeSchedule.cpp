// matth-x/MicroOcpp
// Copyright Matthias Akstaller 2019 - 2024
// MIT License

#include <MicroOcpp/Operations/GetCompositeSchedule.h>
#include <MicroOcpp/Model/Model.h>
#include <MicroOcpp/Model/SmartCharging/SmartChargingService.h>
#include <MicroOcpp/Debug.h>

using MicroOcpp::Ocpp16::GetCompositeSchedule;
using MicroOcpp::MemJsonDoc;

GetCompositeSchedule::GetCompositeSchedule(Model& model, SmartChargingService& scService) : AllocOverrider("v16.Operation.", getOperationType()), model(model), scService(scService) {

}

const char* GetCompositeSchedule::getOperationType() {
    return "GetCompositeSchedule";
}

void GetCompositeSchedule::processReq(JsonObject payload) {

    connectorId = payload["connectorId"] | -1;
    duration = payload["duration"] | 0;

    if (connectorId < 0 || !payload.containsKey("duration")) {
        errorCode = "FormationViolation";
        return;
    }

    if ((unsigned int) connectorId >= model.getNumConnectors()) {
        errorCode = "PropertyConstraintViolation";
    }

    const char *unitStr =  payload["chargingRateUnit"] | "_Undefined";

    if (unitStr[0] == 'A' || unitStr[0] == 'a') {
        chargingRateUnit = ChargingRateUnitType_Optional::Amp;
    } else if (unitStr[0] == 'W' || unitStr[0] == 'w') {
        chargingRateUnit = ChargingRateUnitType_Optional::Watt;
    }
}

std::unique_ptr<MemJsonDoc> GetCompositeSchedule::createConf(){

    bool success = false;

    auto chargingSchedule = scService.getCompositeSchedule((unsigned int) connectorId, duration, chargingRateUnit);
    MemJsonDoc chargingScheduleDoc {0};

    if (chargingSchedule) {
        success = chargingSchedule->toJson(chargingScheduleDoc);
    }

    char scheduleStart_str [JSONDATE_LENGTH + 1] = {'\0'};

    if (success && chargingSchedule) {
        success = chargingSchedule->startSchedule.toJsonString(scheduleStart_str, JSONDATE_LENGTH + 1);
    }

    if (success && chargingSchedule) {
        auto doc = makeMemJsonDoc(
                        JSON_OBJECT_SIZE(4) +
                        chargingScheduleDoc.memoryUsage(),
                        getMemoryTag());
        JsonObject payload = doc->to<JsonObject>();
        payload["status"] = "Accepted";
        payload["connectorId"] = connectorId;
        payload["scheduleStart"] = scheduleStart_str;
        payload["chargingSchedule"] = chargingScheduleDoc;
        return doc;
    } else {
        auto doc = makeMemJsonDoc(JSON_OBJECT_SIZE(1), getMemoryTag());
        JsonObject payload = doc->to<JsonObject>();
        payload["status"] = "Rejected";
        return doc;
    }
}
