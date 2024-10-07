// matth-x/MicroOcpp
// Copyright Matthias Akstaller 2019 - 2024
// MIT License

#include <MicroOcpp/Operations/ChangeAvailability.h>
#include <MicroOcpp/Model/Model.h>
#include <MicroOcpp/Model/ConnectorBase/Connector.h>
#include <MicroOcpp/Version.h>

#include <functional>

namespace MicroOcpp {
namespace Ocpp16 {

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

} // namespace Ocpp16
} // namespace MicroOcpp

#if MO_ENABLE_V201

#include <MicroOcpp/Model/Availability/AvailabilityService.h>

namespace MicroOcpp {
namespace Ocpp201 {

ChangeAvailability::ChangeAvailability(AvailabilityService& availabilityService) : MemoryManaged("v201.Operation.", "ChangeAvailability"), availabilityService(availabilityService) {

}

const char* ChangeAvailability::getOperationType(){
    return "ChangeAvailability";
}

void ChangeAvailability::processReq(JsonObject payload) {

    unsigned int evseId = 0;
    
    if (payload.containsKey("evse")) {
        int evseIdRaw = payload["evse"]["id"] | -1;
        if (evseIdRaw < 0) {
            errorCode = "FormationViolation";
            return;
        }
        evseId = (unsigned int)evseIdRaw;

        if ((payload["evse"]["connectorId"] | 1) != 1) {
            errorCode = "PropertyConstraintViolation";
            return;
        }
    }

    auto availabilityEvse = availabilityService.getEvse(evseId);
    if (!availabilityEvse) {
        errorCode = "PropertyConstraintViolation";
        return;
    }

    const char *type = payload["operationalStatus"] | "_Undefined";

    bool operative = false;

    if (!strcmp(type, "Operative")) {
        operative = true;
    } else if (!strcmp(type, "Inoperative")) {
        operative = false;
    } else {
        errorCode = "PropertyConstraintViolation";
        return;
    }

    status = availabilityEvse->changeAvailability(operative);
}

std::unique_ptr<JsonDoc> ChangeAvailability::createConf(){
    auto doc = makeJsonDoc(getMemoryTag(), JSON_OBJECT_SIZE(1));
    JsonObject payload = doc->to<JsonObject>();

    switch (status) {
        case ChangeAvailabilityStatus::Accepted:
            payload["status"] = "Accepted";
            break;
        case ChangeAvailabilityStatus::Scheduled:
            payload["status"] = "Scheduled";
            break;
        case ChangeAvailabilityStatus::Rejected:
            payload["status"] = "Rejected";
            break;
    }

    return doc;
}

} // namespace Ocpp201
} // namespace MicroOcpp

#endif //MO_ENABLE_V201
