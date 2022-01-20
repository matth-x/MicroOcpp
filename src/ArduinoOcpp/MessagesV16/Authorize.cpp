// matth-x/ArduinoOcpp
// Copyright Matthias Akstaller 2019 - 2022
// MIT License

#include <ArduinoOcpp/MessagesV16/Authorize.h>
#include <ArduinoOcpp/Core/OcppModel.h>
#include <ArduinoOcpp/Tasks/ChargePointStatus/ChargePointStatusService.h>

#include <Variants.h>

using ArduinoOcpp::Ocpp16::Authorize;

Authorize::Authorize() {
    this->idTag = String("A0-00-00-00"); //Use a default payload. In the typical use case of this library, you probably you don't even need Authorization at all
}

Authorize::Authorize(const String &idTag) {
    this->idTag = String(idTag);
}

const char* Authorize::getOcppOperationType(){
    return "Authorize";
}

std::unique_ptr<DynamicJsonDocument> Authorize::createReq() {
    auto doc = std::unique_ptr<DynamicJsonDocument>(new DynamicJsonDocument(JSON_OBJECT_SIZE(1) + (idTag.length() + 1)));
    JsonObject payload = doc->to<JsonObject>();
    payload["idTag"] = idTag;
    return doc;
}

void Authorize::processConf(JsonObject payload){
    String idTagInfo = payload["idTagInfo"]["status"] | "Invalid";

    if (idTagInfo.equals("Accepted")) {
        if (DEBUG_OUT) Serial.print(F("[Authorize] Request has been accepted!\n"));

        if (ocppModel && ocppModel->getChargePointStatusService()) {
            ocppModel->getChargePointStatusService()->authorize(idTag);
        }
    
    } else {
        Serial.print(F("[Authorize] Request has been denied!"));
    }
}

void Authorize::processReq(JsonObject payload){
    /*
     * Ignore Contents of this Req-message, because this is for debug purposes only
     */
}

std::unique_ptr<DynamicJsonDocument> Authorize::createConf(){
    auto doc = std::unique_ptr<DynamicJsonDocument>(new DynamicJsonDocument(2 * JSON_OBJECT_SIZE(1)));
    JsonObject payload = doc->to<JsonObject>();
    JsonObject idTagInfo = payload.createNestedObject("idTagInfo");
    idTagInfo["status"] = "Accepted";
    return doc;
}
