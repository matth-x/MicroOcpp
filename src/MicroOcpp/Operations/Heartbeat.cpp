// matth-x/MicroOcpp
// Copyright Matthias Akstaller 2019 - 2023
// MIT License

#include <MicroOcpp/Operations/Heartbeat.h>
#include <MicroOcpp/Model/Model.h>
#include <MicroOcpp/Debug.h>
#include <string.h>

using MicroOcpp::Ocpp16::Heartbeat;

Heartbeat::Heartbeat(Model& model) : model(model) {
  
}

const char* Heartbeat::getOperationType(){
    return "Heartbeat";
}

std::unique_ptr<DynamicJsonDocument> Heartbeat::createReq() {
    return createEmptyDocument();
}

void Heartbeat::processConf(JsonObject payload) {
  
    const char* currentTime = payload["currentTime"] | "Invalid";
    if (strcmp(currentTime, "Invalid")) {
        if (model.getClock().setTime(currentTime)) {
            //success
            MO_DBG_DEBUG("Request has been accepted");
        } else {
            MO_DBG_WARN("Could not read time string. Expect format like 2020-02-01T20:53:32.486Z");
        }
    } else {
        MO_DBG_WARN("Missing field currentTime. Expect format like 2020-02-01T20:53:32.486Z");
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
    Timestamp ocppTimeReference = Timestamp(2019,10,0,11,59,55); 
    Timestamp ocppSelect = ocppTimeReference;
    auto& ocppNow = model.getClock().now();
    if (ocppNow > ocppTimeReference) {
        //time has already been set
        ocppSelect = ocppNow;
    }

    char ocppNowJson [JSONDATE_LENGTH + 1] = {'\0'};
    ocppSelect.toJsonString(ocppNowJson, JSONDATE_LENGTH + 1);
    payload["currentTime"] = ocppNowJson;

    return doc;
}
