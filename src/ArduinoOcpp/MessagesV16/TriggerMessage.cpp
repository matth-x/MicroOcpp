// matth-x/ArduinoOcpp
// Copyright Matthias Akstaller 2019 - 2022
// MIT License

#include <ArduinoOcpp/MessagesV16/TriggerMessage.h>
#include <ArduinoOcpp/SimpleOcppOperationFactory.h>
#include <ArduinoOcpp/Core/OcppEngine.h>
#include <Variants.h>

using ArduinoOcpp::Ocpp16::TriggerMessage;

TriggerMessage::TriggerMessage() {
    statusMessage = "NotImplemented"; //default value if anything goes wrong
}

const char* TriggerMessage::getOcppOperationType(){
    return "TriggerMessage";
}

void TriggerMessage::processReq(JsonObject payload) {

    Serial.println(F("[TriggerMessage] Warning: TriggerMessage is not tested!"));

    triggeredOperation = makeFromTriggerMessage(payload);
    if (triggeredOperation != nullptr) {
        statusMessage = "Accepted";
    } else {
        Serial.println(F("[TriggerMessage] Couldn't make OppOperation from TriggerMessage. Ignore request."));
        statusMessage = "NotImplemented";
    }
}

std::unique_ptr<DynamicJsonDocument> TriggerMessage::createConf(){
    auto doc = std::unique_ptr<DynamicJsonDocument>(new DynamicJsonDocument(JSON_OBJECT_SIZE(1) + strlen(statusMessage)));
    JsonObject payload = doc->to<JsonObject>();
    
    payload["status"] = statusMessage;
    
    if (triggeredOperation && defaultOcppEngine) //from the second createConf()-try on, do not initiate further OCPP ops
        defaultOcppEngine->initiateOperation(std::move(triggeredOperation));

    return doc;
}
