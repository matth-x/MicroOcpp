// matth-x/MicroOcpp
// Copyright Matthias Akstaller 2019 - 2024
// MIT License

#include <MicroOcpp/Operations/ChangeAvailability.h>
#include <MicroOcpp/Model/Model.h>
#include <MicroOcpp/Model/ConnectorBase/Connector.h>
#include <MicroOcpp/Version.h>

#include <functional>

using MicroOcpp::Ocpp16::ChangeAvailability;
using MicroOcpp::JsonDoc;

ChangeAvailability::ChangeAvailability(Model& model) : MemoryManaged("v16.Operation.", "ChangeAvailability"), model(model) {

}

const char* ChangeAvailability::getOperationType(){
    return "ChangeAvailability";
}

void ChangeAvailability::processReq(JsonObject payload) {
    int connectorIdRaw = payload["connectorId"] | -1;
    if (connectorIdRaw < 0) {
        errorCode = "FormationViolation";
        return;
    }
    unsigned int connectorId = (unsigned int)connectorIdRaw;

    if (connectorId >= model.getNumConnectors()) {
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
        errorCode = "PropertyConstraintViolation";
        return;
    }

    if (connectorId == 0) {
        for (unsigned int cId = 0; cId < model.getNumConnectors(); cId++) {
            auto connector = model.getConnector(cId);
            connector->setAvailability(available);
            if (connector->isOperative() && !available) {
                scheduled = true;
            }
        }
    } else {
        auto connector = model.getConnector(connectorId);
        connector->setAvailability(available);
        if (connector->isOperative() && !available) {
            scheduled = true;
        }
    }
}

std::unique_ptr<JsonDoc> ChangeAvailability::createConf(){
    auto doc = makeJsonDoc(getMemoryTag(), JSON_OBJECT_SIZE(1));
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
