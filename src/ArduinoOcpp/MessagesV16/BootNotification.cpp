// matth-x/ArduinoOcpp
// Copyright Matthias Akstaller 2019 - 2021
// MIT License

#include <Variants.h>

#include <ArduinoOcpp/MessagesV16/BootNotification.h>
#include <ArduinoOcpp/Core/OcppModel.h>
#include <ArduinoOcpp/Tasks/ChargePointStatus/ChargePointStatusService.h>
#include <ArduinoOcpp/Core/Configuration.h>

#include <string.h>

using ArduinoOcpp::Ocpp16::BootNotification;

BootNotification::BootNotification() {
  
}

BootNotification::BootNotification(String &cpModel, String &cpVendor) {
    chargePointModel = String(cpModel);
    chargePointVendor = String(cpVendor);
}

BootNotification::BootNotification(String &cpModel, String &cpVendor, String &cpSerialNumber) {
    chargePointModel = String(cpModel);
    chargePointVendor = String(cpVendor);
    chargePointSerialNumber = String(cpSerialNumber);
}

BootNotification::BootNotification(String &cpModel, String &cpVendor, String &cpSerialNumber, String &fwVersion) {
    chargePointModel = String(cpModel);
    chargePointVendor = String(cpVendor);
    chargePointSerialNumber = String(cpSerialNumber);
    firmwareVersion = String(fwVersion);
}

BootNotification::BootNotification(DynamicJsonDocument *payload) {
    this->overridePayload = payload;
}

BootNotification::~BootNotification() {
    if (overridePayload != nullptr)
        delete overridePayload;
}

const char* BootNotification::getOcppOperationType(){
    return "BootNotification";
}

std::unique_ptr<DynamicJsonDocument> BootNotification::createReq() {

    if (overridePayload != nullptr) {
        auto result = std::unique_ptr<DynamicJsonDocument>(new DynamicJsonDocument(*overridePayload));
        return result;
    }

    auto doc = std::unique_ptr<DynamicJsonDocument>(new DynamicJsonDocument(JSON_OBJECT_SIZE(4)
        + chargePointModel.length() + 1
        + chargePointVendor.length() + 1
        + chargePointSerialNumber.length() + 1
        + firmwareVersion.length() + 1));
    JsonObject payload = doc->to<JsonObject>();
    payload["chargePointModel"] = chargePointModel;
    payload["chargePointVendor"] = chargePointVendor;
    if (!chargePointSerialNumber.isEmpty()) {
        payload["chargePointSerialNumber"] = chargePointSerialNumber;
    }
    if (!firmwareVersion.isEmpty()) {
        payload["firmwareVersion"] = firmwareVersion;
    }
    return doc;
}

void BootNotification::processConf(JsonObject payload){
    const char* currentTime = payload["currentTime"] | "Invalid";
    if (strcmp(currentTime, "Invalid")) {
        if (ocppModel && ocppModel->getOcppTime().setOcppTime(currentTime)) {
            //success
        } else {
            Serial.print(F("[BootNotification] Error reading time string. Expect format like 2020-02-01T20:53:32.486Z\n"));
        }
    } else {
        Serial.print(F("[BootNotification] Error reading time string. Missing attribute currentTime of type string\n"));
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
        if (DEBUG_OUT) Serial.print(F("[BootNotification] Request has been accepted!\n"));
        if (ocppModel && ocppModel->getChargePointStatusService() != nullptr) {
            ocppModel->getChargePointStatusService()->boot();
        }
    } else {
        Serial.print(F("[BootNotification] Request unsuccessful!\n"));
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
    OcppTimestamp ocppTimeReference = OcppTimestamp(2019,10,0,11,59,55); 
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
