// matth-x/ArduinoOcpp
// Copyright Matthias Akstaller 2019 - 2021
// MIT License

#include <ArduinoOcpp/MessagesV16/Heartbeat.h>
#include <ArduinoOcpp/Core/OcppModel.h>
#include <string.h>
#include <Variants.h>

using ArduinoOcpp::Ocpp16::Heartbeat;

Heartbeat::Heartbeat()  {
  
}

const char* Heartbeat::getOcppOperationType(){
    return "Heartbeat";
}

std::unique_ptr<DynamicJsonDocument> Heartbeat::createReq() {
    return createEmptyDocument();
}

void Heartbeat::processConf(JsonObject payload) {
  
    const char* currentTime = payload["currentTime"] | "Invalid";
    if (strcmp(currentTime, "Invalid")) {
        if (ocppModel && ocppModel->getOcppTime().setOcppTime(currentTime)) {
            //success
            if (DEBUG_OUT) Serial.print(F("[Heartbeat] Request has been accepted!\n"));
        } else {
            Serial.print(F("[Heartbeat] Request accepted. But Error reading time string. Expect format like 2020-02-01T20:53:32.486Z\n"));
        }
    } else {
        Serial.print(F("[Heartbeat] Request denied. Missing field currentTime. Expect format like 2020-02-01T20:53:32.486Z\n"));
    }
}

void Heartbeat::processReq(JsonObject payload) {

    /**
     * Ignore Contents of this Req-message, because this is for debug purposes only
     */

}

std::unique_ptr<DynamicJsonDocument> Heartbeat::createConf(){
    auto doc = std::unique_ptr<DynamicJsonDocument>(new DynamicJsonDocument(JSON_OBJECT_SIZE(1) + (JSONDATE_LENGTH + 1)));
    JsonObject payload = doc->to<JsonObject>();

    //safety mechanism; in some test setups the library could have to answer Heartbeats without valid system time
    OcppTimestamp ocppTimeReference = OcppTimestamp(2019,10,0,11,59,55); 
    OcppTimestamp ocppSelect = ocppTimeReference;
    if (ocppModel) {
        auto& ocppNow = ocppModel->getOcppTime().getOcppTimestampNow();
        if (ocppNow > ocppTimeReference) {
            //time has already been set
            ocppSelect = ocppNow;
        }
    }

    char ocppNowJson [JSONDATE_LENGTH + 1] = {'\0'};
    ocppSelect.toJsonString(ocppNowJson, JSONDATE_LENGTH + 1);
    payload["currentTime"] = ocppNowJson;

    return doc;
}
