// matth-x/ArduinoOcpp
// Copyright Matthias Akstaller 2019 - 2022
// MIT License

#include <ArduinoOcpp/MessagesV16/BootNotification.h>
#include <ArduinoOcpp/Core/OcppModel.h>
#include <ArduinoOcpp/Tasks/ChargePointStatus/ChargePointStatusService.h>
#include <ArduinoOcpp/Core/Configuration.h>
#include <ArduinoOcpp/Debug.h>

#include <string.h>

using ArduinoOcpp::Ocpp16::BootNotification;

BootNotification::BootNotification() {
    
}

BootNotification::BootNotification(std::unique_ptr<DynamicJsonDocument> payload) : credentials(std::move(payload)) {
    
}

const char* BootNotification::getOcppOperationType(){
    return "BootNotification";
}

void BootNotification::initiate() {
    if (credentials &&
            ocppModel && ocppModel->getChargePointStatusService()) {
        auto cpStatus = ocppModel->getChargePointStatusService();
        cpStatus->setChargePointCredentials(*credentials);
        credentials.release();
    }
}

std::unique_ptr<DynamicJsonDocument> BootNotification::createReq() {

    if (ocppModel && ocppModel->getChargePointStatusService()) {
        auto cpStatus = ocppModel->getChargePointStatusService();
        const auto& cpCredentials = cpStatus->getChargePointCredentials();
    
        std::unique_ptr<DynamicJsonDocument> doc;
        size_t capacity = JSON_OBJECT_SIZE(9) + cpCredentials.size();
        DeserializationError err = DeserializationError::NoMemory;
        while (err == DeserializationError::NoMemory) {
            doc.reset(new DynamicJsonDocument(capacity));
            err = deserializeJson(*doc, cpCredentials);

            capacity *= 3;
            capacity /= 2;
        }

        if (!err) {
            return doc;
        } else {
            AO_DBG_ERR("could not parse stored credentials: %s", err.c_str());
        }
    }

    if (credentials) {
        return std::unique_ptr<DynamicJsonDocument>(new DynamicJsonDocument(*credentials));
    }
    
    AO_DBG_ERR("payload undefined");
    return createEmptyDocument();
}

void BootNotification::processConf(JsonObject payload){
    const char* currentTime = payload["currentTime"] | "Invalid";
    if (strcmp(currentTime, "Invalid")) {
        if (ocppModel && ocppModel->getOcppTime().setOcppTime(currentTime)) {
            //success
        } else {
            AO_DBG_ERR("Time string format violation. Expect format like 2022-02-01T20:53:32.486Z");
        }
    } else {
        AO_DBG_ERR("Missing attribute currentTime");
    }
    
    int interval = payload["interval"] | -1;

    //only write if in valid range
    if (interval >= 1) {
        std::shared_ptr<Configuration<int>> intervalConf = declareConfiguration<int>("HeartbeatInterval", 86400);
        if (intervalConf && interval != *intervalConf) {
            *intervalConf = interval;
            configuration_save();
        }
    }

    const char* status = payload["status"] | "Invalid";

    if (!strcmp(status, "Accepted")) {
        AO_DBG_INFO("Request has been accepted");
        if (ocppModel && ocppModel->getChargePointStatusService() != nullptr) {
            ocppModel->getChargePointStatusService()->boot();
        }
    } else {
        AO_DBG_WARN("Request unsuccessful");
    }
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
    OcppTimestamp ocppTimeReference = OcppTimestamp(2022,0,27,11,59,55); 
    OcppTimestamp ocppSelect = ocppTimeReference;
    if (ocppModel) {
        auto& ocppTime = ocppModel->getOcppTime();
        OcppTimestamp ocppNow = ocppTime.getOcppTimestampNow();
        if (ocppNow > ocppTimeReference) {
            //time has already been set
            ocppSelect = ocppNow;
        }
    }

    char ocppNowJson [JSONDATE_LENGTH + 1] = {'\0'};
    ocppSelect.toJsonString(ocppNowJson, JSONDATE_LENGTH + 1);
    payload["currentTime"] = ocppNowJson;

    payload["interval"] = 86400; //heartbeat send interval - not relevant for JSON variant of OCPP so send dummy value that likely won't break
    payload["status"] = "Accepted";
    return doc;
}
