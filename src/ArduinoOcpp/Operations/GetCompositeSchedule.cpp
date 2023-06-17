// matth-x/ArduinoOcpp
// Copyright Matthias Akstaller 2019 - 2023
// MIT License

#include <ArduinoOcpp/MessagesV16/GetCompositeSchedule.h>
#include <ArduinoOcpp/Core/Model.h>
#include <ArduinoOcpp/Tasks/ChargeControl/Connector.h>
#include <ArduinoOcpp/Tasks/SmartCharging/SmartChargingService.h>
#include <ArduinoOcpp/Debug.h>

#include <functional>

using ArduinoOcpp::Ocpp16::GetCompositeSchedule;

GetCompositeSchedule::GetCompositeSchedule(Model& model) : model(model) {

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

    auto unitString =  payload["chargingRateUnit"] | "W";

    if (unitString[0] == 'A' || unitString[0] == 'a') {
        chargingRateUnit = ChargingRateUnitType::Amp;
    } else if (unitString[0] == 'W' || unitString[0] == 'w') {
        chargingRateUnit = ChargingRateUnitType::Watt;
    } else {
        errorCode = "PropertyConstraintViolation";
    }

    if (!model.getSmartChargingService()) {
        AO_DBG_ERR("SmartChargingService not initialized! Ignore request");
        errorCode = "NotSupported";
    }
}

std::unique_ptr<DynamicJsonDocument> GetCompositeSchedule::createConf(){
    auto scService = model.getSmartChargingService();
    if (!scService) {
        AO_DBG_ERR("invalid state");
        return createEmptyDocument();
    }

    ChargingSchedule *composite = scService->getCompositeSchedule((unsigned int) connectorId, duration);
    DynamicJsonDocument *compositeJson {nullptr};

    if (composite) {
        compositeJson = composite->toJsonDocument();
    }

    std::unique_ptr<DynamicJsonDocument> doc;

    if (compositeJson) {
        doc.reset(new DynamicJsonDocument(JSON_OBJECT_SIZE(4) + JSONDATE_LENGTH + 1 + compositeJson->capacity()));
        JsonObject payload = doc->to<JsonObject>();
        payload["status"] = "Accepted";
        if (connectorId > 0)
            payload["connectorId"] = connectorId;

        char scheduleStart [JSONDATE_LENGTH + 1] {'\0'};
        auto startSchedule = (*compositeJson)["startSchedule"] | "";
        if (startSchedule[0] != '\0') {
            strncpy(scheduleStart, startSchedule, JSONDATE_LENGTH + 1);
        } else {
            model.getClock().now().toJsonString(scheduleStart, JSONDATE_LENGTH + 1);
        }
        payload["scheduleStart"] = scheduleStart;
        
        payload["chargingSchedule"] = *compositeJson;
    } else {
        doc.reset(new DynamicJsonDocument(JSON_OBJECT_SIZE(1)));
        JsonObject payload = doc->to<JsonObject>();
        payload["status"] = "Rejected";
    }

    delete compositeJson;
    delete composite;

    return doc;
}
