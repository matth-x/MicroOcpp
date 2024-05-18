// matth-x/MicroOcpp
// Copyright Matthias Akstaller 2019 - 2024
// MIT License

#include <MicroOcpp/Operations/ChangeAvailability.h>
#include <MicroOcpp/Model/Model.h>
#include <MicroOcpp/Model/ConnectorBase/Connector.h>
#include <MicroOcpp/Version.h>

#include <functional>

using MicroOcpp::Ocpp16::ChangeAvailability;

ChangeAvailability::ChangeAvailability(Model& model) : model(model) {

}

const char* ChangeAvailability::getOperationType(){
    return "ChangeAvailability";
}

void ChangeAvailability::processReq(JsonObject payload) {

    unsigned int connectorId = 0;
    const char *type = "_Undefined";

    #if MO_ENABLE_V201
    if (model.getVersion().major == 2) {
        //OCPP 2.0.1
        int connectorIdRaw = payload["evse"]["id"] | 0;
        if (connectorIdRaw < 0) {
            errorCode = "FormationViolation";
            return;
        }

        connectorId = (unsigned int)connectorIdRaw;
        type = payload["operationalStatus"] | type;
    } else
    #endif //MO_ENABLE_V201
    {
        //OCPP 1.6
        int connectorIdRaw = payload["connectorId"] | -1;
        if (connectorIdRaw < 0) {
            errorCode = "FormationViolation";
            return;
        }

        connectorId = (unsigned int)connectorIdRaw;
        type = payload["type"] | type;
    }

    if (connectorId >= model.getNumConnectors()) {
        errorCode = "PropertyConstraintViolation";
        return;
    }

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
