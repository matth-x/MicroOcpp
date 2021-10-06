// matth-x/ArduinoOcpp
// Copyright Matthias Akstaller 2019 - 2021
// MIT License

#include <ArduinoOcpp/MessagesV16/ChangeAvailability.h>
#include <ArduinoOcpp/Core/OcppEngine.h>

#include <functional>

using ArduinoOcpp::Ocpp16::ChangeAvailability;

ChangeAvailability::ChangeAvailability() {

}

const char* ChangeAvailability::getOcppOperationType(){
    return "ChangeAvailability";
}

void ChangeAvailability::processReq(JsonObject payload) {
    int connectorId = payload["connectorId"] | -1;
    ChargePointStatusService *cpStatus = getChargePointStatusService();
    if (!cpStatus) {
        return;
    }
    if (connectorId < 0 || connectorId >= cpStatus->getNumConnectors()) {
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
        for (int i = 0; i < cpStatus->getNumConnectors(); i++) {
            cpStatus->getConnector(i)->setAvailability(available);
            if (cpStatus->getConnector(i)->getAvailability() == AVAILABILITY_INOPERATIVE_SCHEDULED) {
                scheduled = true;
            }
        }
    } else {
        cpStatus->getConnector(connectorId)->setAvailability(available);
        if (cpStatus->getConnector(connectorId)->getAvailability() == AVAILABILITY_INOPERATIVE_SCHEDULED) {
            scheduled = true;
        }
    }
}

DynamicJsonDocument* ChangeAvailability::createConf(){
    DynamicJsonDocument* doc = new DynamicJsonDocument(JSON_OBJECT_SIZE(1));
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
