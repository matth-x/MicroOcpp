// matth-x/ArduinoOcpp
// Copyright Matthias Akstaller 2019 - 2023
// MIT License

#include <ArduinoOcpp/Operations/TriggerMessage.h>
#include <ArduinoOcpp/Model/ChargeControl/Connector.h>
#include <ArduinoOcpp/Model/Metering/MeteringService.h>
#include <ArduinoOcpp/Operations/StatusNotification.h>
#include <ArduinoOcpp/Model/Model.h>
#include <ArduinoOcpp/Context.h>
#include <ArduinoOcpp/Core/SimpleRequestFactory.h>
#include <ArduinoOcpp/Debug.h>

using ArduinoOcpp::Ocpp16::TriggerMessage;

TriggerMessage::TriggerMessage(Context& context) : context(context) {

}

const char* TriggerMessage::getOperationType(){
    return "TriggerMessage";
}

void TriggerMessage::processReq(JsonObject payload) {

    const char *requestedMessage = payload["requestedMessage"] | "Invalid";
    const int connectorId = payload["connectorId"] | -1;

    AO_DBG_INFO("Execute for message type %s, connectorId = %i", requestedMessage, connectorId);

    statusMessage = "Rejected";

    if (!strcmp(requestedMessage, "MeterValues")) {
        if (auto mService = context.getModel().getMeteringService()) {
            if (connectorId < 0) {
                auto nConnectors = mService->getNumConnectors();
                for (decltype(nConnectors) cId = 0; cId < nConnectors; cId++) {
                    triggeredOperations.push_back(mService->takeTriggeredMeterValues(cId));
                }
            } else if (connectorId < mService->getNumConnectors()) {
                triggeredOperations.push_back(mService->takeTriggeredMeterValues(connectorId));
            } else {
                errorCode = "PropertyConstraintViolation";
            }
        }
    } else if (!strcmp(requestedMessage, "StatusNotification")) {
        unsigned int cIdRangeBegin = 0, cIdRangeEnd = 0;
        if (connectorId < 0) {
            cIdRangeEnd = context.getModel().getNumConnectors();
        } else if ((unsigned int) connectorId < context.getModel().getNumConnectors()) {
            cIdRangeBegin = connectorId;
            cIdRangeEnd = connectorId + 1;
        } else {
            errorCode = "PropertyConstraintViolation";
        }

        for (auto i = cIdRangeBegin; i < cIdRangeEnd; i++) {
            auto connector = context.getModel().getConnector(i);

            auto statusNotification = makeRequest(new Ocpp16::StatusNotification(
                        i,
                        connector->inferenceStatus(), //will be determined in StatusNotification::initiate
                        context.getModel().getClock().now()));

            statusNotification->setTimeout(60000);

            triggeredOperations.push_back(std::move(statusNotification));
        }
    } else {
        auto msg = context.getOperationRegistry().deserializeOperation(requestedMessage);
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

    auto op = triggeredOperations.begin();
    while (op != triggeredOperations.end()) {
        context.initiatePreBootOperation(std::move(triggeredOperations.front()));
        op = triggeredOperations.erase(op);
    }

    return doc;
}
