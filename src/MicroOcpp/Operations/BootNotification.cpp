// matth-x/MicroOcpp
// Copyright Matthias Akstaller 2019 - 2023
// MIT License

#include <MicroOcpp/Operations/BootNotification.h>
#include <MicroOcpp/Model/Model.h>
#include <MicroOcpp/Model/Boot/BootService.h>
#include <MicroOcpp/Core/Configuration.h>
#include <MicroOcpp/Debug.h>

#include <string.h>

using MicroOcpp::Ocpp16::BootNotification;

BootNotification::BootNotification(Model& model, std::unique_ptr<DynamicJsonDocument> payload) : model(model), credentials(std::move(payload)) {
    
}

const char* BootNotification::getOperationType(){
    return "BootNotification";
}

std::unique_ptr<DynamicJsonDocument> BootNotification::createReq() {
    if (credentials) {
        return std::unique_ptr<DynamicJsonDocument>(new DynamicJsonDocument(*credentials));
    } else {
        MO_DBG_ERR("payload undefined");
        return createEmptyDocument();
    }
}

void BootNotification::processConf(JsonObject payload) {
    const char* currentTime = payload["currentTime"] | "Invalid";
    if (strcmp(currentTime, "Invalid")) {
        if (model.getClock().setTime(currentTime)) {
            //success
        } else {
            MO_DBG_ERR("Time string format violation. Expect format like 2022-02-01T20:53:32.486Z");
            errorCode = "PropertyConstraintViolation";
            return;
        }
    } else {
        MO_DBG_ERR("Missing attribute currentTime");
        errorCode = "FormationViolation";
        return;
    }
    
    int interval = payload["interval"] | -1;
    if (interval < 0) {
        errorCode = "FormationViolation";
        return;
    }

    RegistrationStatus status = deserializeRegistrationStatus(payload["status"] | "Invalid");
    
    if (status == RegistrationStatus::UNDEFINED) {
        errorCode = "FormationViolation";
        return;
    }

    if (status == RegistrationStatus::Accepted) {
        //only write if in valid range
        if (interval >= 1) {
            auto heartbeatIntervalInt = declareConfiguration<int>("HeartbeatInterval", 86400);
            if (heartbeatIntervalInt && interval != heartbeatIntervalInt->getInt()) {
                heartbeatIntervalInt->setInt(interval);
                configuration_save();
            }
        }
    }

    if (auto bootService = model.getBootService()) {

        if (status != RegistrationStatus::Accepted) {
            bootService->setRetryInterval(interval);
        }

        bootService->notifyRegistrationStatus(status);
    }

    MO_DBG_INFO("request has been %s", status == RegistrationStatus::Accepted ? "Accepted" :
                                       status == RegistrationStatus::Pending ? "replied with Pending" :
                                       "Rejected");
}

void BootNotification::processReq(JsonObject payload){
    /*
     * Ignore Contents of this Req-message, because this is for debug purposes only
     */
}

std::unique_ptr<DynamicJsonDocument> BootNotification::createConf(){
    auto doc = std::unique_ptr<DynamicJsonDocument>(new DynamicJsonDocument(JSON_OBJECT_SIZE(3) + (JSONDATE_LENGTH + 1)));
    JsonObject payload = doc->to<JsonObject>();

    //safety mechanism; in some test setups the library has to answer BootNotifications without valid system time
    Timestamp ocppTimeReference = Timestamp(2022,0,27,11,59,55); 
    Timestamp ocppSelect = ocppTimeReference;
    auto& ocppTime = model.getClock();
    Timestamp ocppNow = ocppTime.now();
    if (ocppNow > ocppTimeReference) {
        //time has already been set
        ocppSelect = ocppNow;
    }

    char ocppNowJson [JSONDATE_LENGTH + 1] = {'\0'};
    ocppSelect.toJsonString(ocppNowJson, JSONDATE_LENGTH + 1);
    payload["currentTime"] = ocppNowJson;

    payload["interval"] = 86400; //heartbeat send interval - not relevant for JSON variant of OCPP so send dummy value that likely won't break
    payload["status"] = "Accepted";
    return doc;
}
