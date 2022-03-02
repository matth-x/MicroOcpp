// matth-x/ArduinoOcpp
// Copyright Matthias Akstaller 2019 - 2022
// MIT License

#include <ArduinoOcpp/MessagesV16/TriggerMessage.h>
#include <ArduinoOcpp/Tasks/ChargePointStatus/ChargePointStatusService.h>
#include <ArduinoOcpp/Tasks/Metering/MeteringService.h>
#include <ArduinoOcpp/Core/OcppModel.h>
#include <ArduinoOcpp/Core/OcppEngine.h>
#include <ArduinoOcpp/SimpleOcppOperationFactory.h>
#include <ArduinoOcpp/Debug.h>

using ArduinoOcpp::Ocpp16::TriggerMessage;

const char* TriggerMessage::getOcppOperationType(){
    return "TriggerMessage";
}

void TriggerMessage::processReq(JsonObject payload) {

    const char *requestedMessage = payload["requestedMessage"] | "Invalid";
    const int connectorId = payload["connectorId"] | -1;

    AO_DBG_INFO("Execute for message type %s, connectorId = %i", requestedMessage, connectorId);

    statusMessage = "Rejected";

    if (!strcmp(requestedMessage, "MeterValues")) {
        if (ocppModel && ocppModel->getMeteringService()) {
            auto mService = ocppModel->getMeteringService();
            if (connectorId < 0) {
                auto nConnectors = mService->getNumConnectors();
                for (decltype(nConnectors) i = 0; i < nConnectors; i++) {
                    triggeredOperations.push_back(mService->takeMeterValuesNow(i));
                }
            } else if (connectorId < mService->getNumConnectors()) {
                triggeredOperations.push_back(mService->takeMeterValuesNow(connectorId));
            } else {
                errorCode = "PropertyConstraintViolation";
            }
        }
    } else if (!strcmp(requestedMessage, "StatusNotification")) {
        if (ocppModel && ocppModel->getChargePointStatusService()) {
            auto cpsService = ocppModel->getChargePointStatusService();
            if (connectorId < 0) {
                auto nConnectors = cpsService->getNumConnectors();
                for (decltype(nConnectors) i = 0; i < nConnectors; i++) {
                    triggeredOperations.push_back(makeOcppOperation(requestedMessage, i));
                }
            } else if (connectorId < cpsService->getNumConnectors()) {
                triggeredOperations.push_back(makeOcppOperation(requestedMessage, connectorId));
            } else {
                errorCode = "PropertyConstraintViolation";
            }
        }
    } else {
        auto msg = makeOcppOperation(requestedMessage, connectorId);
        if (msg) {
            triggeredOperations.push_back(std::move(msg));
        } else {
            statusMessage = "NotImplemented";
        }
    }

    if (!triggeredOperations.empty()) {
        statusMessage = "Accepted";
    } else {
        if (errorCode) {
            AO_DBG_ERR("errorCode: %s", errorCode);
        } else {
            AO_DBG_WARN("TriggerMessage denied. statusMessage: %s", statusMessage);
        }
    }

}

std::unique_ptr<DynamicJsonDocument> TriggerMessage::createConf(){
    auto doc = std::unique_ptr<DynamicJsonDocument>(new DynamicJsonDocument(JSON_OBJECT_SIZE(1)));
    JsonObject payload = doc->to<JsonObject>();
    
    payload["status"] = statusMessage;

    if (defaultOcppEngine) {
        auto op = triggeredOperations.begin();
        while (op != triggeredOperations.end()) {
            defaultOcppEngine->initiateOperation(std::move(triggeredOperations.front()));
            op = triggeredOperations.erase(op);
        }
    }

    return doc;
}
