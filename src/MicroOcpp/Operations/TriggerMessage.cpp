// matth-x/MicroOcpp
// Copyright Matthias Akstaller 2019 - 2023
// MIT License

#include <MicroOcpp/Operations/TriggerMessage.h>
#include <MicroOcpp/Model/ConnectorBase/Connector.h>
#include <MicroOcpp/Model/Metering/MeteringService.h>
#include <MicroOcpp/Operations/StatusNotification.h>
#include <MicroOcpp/Model/Model.h>
#include <MicroOcpp/Core/Context.h>
#include <MicroOcpp/Core/SimpleRequestFactory.h>
#include <MicroOcpp/Debug.h>

using MicroOcpp::Ocpp16::TriggerMessage;

TriggerMessage::TriggerMessage(Context& context) : context(context) {

}

const char* TriggerMessage::getOperationType(){
    return "TriggerMessage";
}

void TriggerMessage::processReq(JsonObject payload) {

    const char *requestedMessage = payload["requestedMessage"] | "Invalid";
    const int connectorId = payload["connectorId"] | -1;

    MO_DBG_INFO("Execute for message type %s, connectorId = %i", requestedMessage, connectorId);

    statusMessage = "Rejected";

    if (!strcmp(requestedMessage, "MeterValues")) {
        if (auto mService = context.getModel().getMeteringService()) {
            if (connectorId < 0) {
                auto nConnectors = mService->getNumConnectors();
                for (decltype(nConnectors) cId = 0; cId < nConnectors; cId++) {
                    context.initiatePreBootOperation(mService->takeTriggeredMeterValues(cId));
                    statusMessage = "Accepted";
                }
            } else if (connectorId < mService->getNumConnectors()) {
                context.initiatePreBootOperation(mService->takeTriggeredMeterValues(connectorId));
                statusMessage = "Accepted";
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
                        connector->getStatus(), //will be determined in StatusNotification::initiate
                        context.getModel().getClock().now()));

            statusNotification->setTimeout(60000);

            context.initiatePreBootOperation(std::move(statusNotification));
            statusMessage = "Accepted";
        }
    } else {
        auto msg = context.getOperationRegistry().deserializeOperation(requestedMessage);
        if (msg) {
            context.initiatePreBootOperation(std::move(msg));
            statusMessage = "Accepted";
        } else {
            statusMessage = "NotImplemented";
        }
    }
}

std::unique_ptr<DynamicJsonDocument> TriggerMessage::createConf(){
    auto doc = std::unique_ptr<DynamicJsonDocument>(new DynamicJsonDocument(JSON_OBJECT_SIZE(1)));
    JsonObject payload = doc->to<JsonObject>();
    payload["status"] = statusMessage;
    return doc;
}
