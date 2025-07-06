// matth-x/MicroOcpp
// Copyright Matthias Akstaller 2019 - 2024
// MIT License

#include <MicroOcpp/Operations/ChangeAvailability.h>

#include <MicroOcpp/Model/Availability/AvailabilityService.h>
#include <MicroOcpp/Model/Model.h>

using namespace MicroOcpp;

#if MO_ENABLE_V16

v16::ChangeAvailability::ChangeAvailability(Model& model) : MemoryManaged("v16.Operation.", "ChangeAvailability"), model(model) {

}

const char* v16::ChangeAvailability::getOperationType(){
    return "ChangeAvailability";
}

void v16::ChangeAvailability::processReq(JsonObject payload) {
    int connectorIdRaw = payload["connectorId"] | -1;
    if (connectorIdRaw < 0) {
        errorCode = "FormationViolation";
        return;
    }
    unsigned int connectorId = (unsigned int)connectorIdRaw;

    if (connectorId >= model.getNumEvseId()) {
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
        for (unsigned int eId = 0; eId < model.getNumEvseId(); eId++) {
            auto availSvc = model.getAvailabilityService();
            auto availSvcEvse = availSvc ? availSvc->getEvse(eId) : nullptr;
            if (availSvcEvse) {
                availSvcEvse->setAvailability(available);
                if (availSvcEvse->isOperative() && !available) {
                    scheduled = true;
                }
            }
        }
    } else {
        auto availSvc = model.getAvailabilityService();
        auto availSvcEvse = availSvc ? availSvc->getEvse(connectorId) : nullptr;
        if (availSvcEvse) {
            availSvcEvse->setAvailability(available);
            if (availSvcEvse->isOperative() && !available) {
                scheduled = true;
            }
        }
    }
}

std::unique_ptr<JsonDoc> v16::ChangeAvailability::createConf(){
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

#endif //MO_ENABLE_V16

#if MO_ENABLE_V201

v201::ChangeAvailability::ChangeAvailability(AvailabilityService& availabilityService) : MemoryManaged("v201.Operation.", "ChangeAvailability"), availabilityService(availabilityService) {

}

const char* v201::ChangeAvailability::getOperationType(){
    return "ChangeAvailability";
}

void v201::ChangeAvailability::processReq(JsonObject payload) {

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

std::unique_ptr<JsonDoc> v201::ChangeAvailability::createConf(){
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

#endif //MO_ENABLE_V201
