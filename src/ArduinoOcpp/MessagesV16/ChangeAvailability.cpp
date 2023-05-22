// matth-x/ArduinoOcpp
// Copyright Matthias Akstaller 2019 - 2023
// MIT License

#include <ArduinoOcpp/MessagesV16/ChangeAvailability.h>
#include <ArduinoOcpp/Core/OcppModel.h>
#include <ArduinoOcpp/Tasks/ChargeControl/Connector.h>

#include <functional>

using ArduinoOcpp::Ocpp16::ChangeAvailability;

ChangeAvailability::ChangeAvailability(OcppModel& context) : context(context) {

}

const char* ChangeAvailability::getOcppOperationType(){
    return "ChangeAvailability";
}

void ChangeAvailability::processReq(JsonObject payload) {
    int connectorIdRaw = payload["connectorId"] | -1;
    if (connectorIdRaw < 0) {
        errorCode = "FormationViolation";
        return;
    }
    unsigned int connectorId = (unsigned int) connectorIdRaw;

    if (connectorId >= context.getNumConnectors()) {
        errorCode = "PropertyConstraintViolation";
        return;
    }

    const char *type = payload["type"] | "INVALID";
    bool available = false;

    if (!strcmp(type, "Operative")) {
        accepted = true;
        available = true;
    } else if (!strcmp(type, "Inoperative")) {
        accepted = true;
        available = false;
    } else {
        return;
    }

    if (connectorId == 0) {
        for (unsigned int cId = 0; cId < context.getNumConnectors(); cId++) {
            auto connector = context.getConnector(cId);
            connector->setAvailability(available);
            if (connector->getAvailability() == AVAILABILITY_INOPERATIVE_SCHEDULED) {
                scheduled = true;
            }
        }
    } else {
        auto connector = context.getConnector(connectorId);
        connector->setAvailability(available);
        if (connector->getAvailability() == AVAILABILITY_INOPERATIVE_SCHEDULED) {
            scheduled = true;
        }
    }
}

std::unique_ptr<DynamicJsonDocument> ChangeAvailability::createConf(){
    auto doc = std::unique_ptr<DynamicJsonDocument>(new DynamicJsonDocument(JSON_OBJECT_SIZE(1)));
    JsonObject payload = doc->to<JsonObject>();
    if (!accepted) {
        payload["status"] = "Rejected";
    } else if (scheduled) {
        payload["status"] = "Scheduled";
    } else {
        payload["status"] = "Accepted";
    }
        
    return doc;
}
