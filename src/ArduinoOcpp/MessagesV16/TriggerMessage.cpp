// matth-x/ArduinoOcpp
// Copyright Matthias Akstaller 2019 - 2022
// MIT License

#include <ArduinoOcpp/MessagesV16/TriggerMessage.h>
#include <ArduinoOcpp/Tasks/ChargePointStatus/ChargePointStatusService.h>
#include <ArduinoOcpp/Core/OcppModel.h>
#include <ArduinoOcpp/Core/OcppEngine.h>
#include <ArduinoOcpp/SimpleOcppOperationFactory.h>
#include <ArduinoOcpp/Debug.h>

using ArduinoOcpp::Ocpp16::TriggerMessage;

TriggerMessage::TriggerMessage() {
    statusMessage = "NotImplemented"; //default value if anything goes wrong
}

const char* TriggerMessage::getOcppOperationType(){
    return "TriggerMessage";
}

void TriggerMessage::processReq(JsonObject payload) {

    AO_DBG_INFO("Warning: TriggerMessage is not fully tested");

    const char *requestedMessage = payload["requestedMessage"] | "Invalid";
    const int connectorId = payload["connectorId"] | 0;

    if (ocppModel && ocppModel->getChargePointStatusService()) {
        if (connectorId >= ocppModel->getChargePointStatusService()->getNumConnectors()) {
            formatError = true;
        }
    }

    if (!formatError) {
        AO_DBG_INFO("Execute for message type %s, connectorId = %i", requestedMessage, connectorId);
        triggeredOperation = makeOcppOperation(requestedMessage, connectorId);
    }

    if (triggeredOperation) {
        statusMessage = "Accepted";
    } else {
        AO_DBG_WARN("Could not make OppOperation from TriggerMessage. Ignore request");
        statusMessage = "NotImplemented";
    }
}

std::unique_ptr<DynamicJsonDocument> TriggerMessage::createConf(){
    auto doc = std::unique_ptr<DynamicJsonDocument>(new DynamicJsonDocument(JSON_OBJECT_SIZE(1)));
    JsonObject payload = doc->to<JsonObject>();
    
    payload["status"] = statusMessage;
    
    if (triggeredOperation && defaultOcppEngine) //from the second createConf()-try on, do not initiate further OCPP ops
        defaultOcppEngine->initiateOperation(std::move(triggeredOperation));

    return doc;
}
