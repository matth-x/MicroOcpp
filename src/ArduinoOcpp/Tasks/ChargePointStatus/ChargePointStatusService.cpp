// matth-x/ArduinoOcpp
// Copyright Matthias Akstaller 2019 - 2022
// MIT License

#include <ArduinoOcpp/Tasks/ChargePointStatus/ChargePointStatusService.h>
#include <ArduinoOcpp/Core/OcppEngine.h>
#include <ArduinoOcpp/SimpleOcppOperationFactory.h>

#include <ArduinoOcpp/Debug.h>

#include <string.h>

using namespace ArduinoOcpp;

ChargePointStatusService::ChargePointStatusService(OcppEngine& context, int numConn)
      : context(context) {

    for (int i = 0; i < numConn; i++) {
        connectors.push_back(std::unique_ptr<ConnectorStatus>(new ConnectorStatus(context.getOcppModel(), i)));
    }
}

ChargePointStatusService::~ChargePointStatusService() {

}

void ChargePointStatusService::loop() {
    if (!booted) return;
    for (auto connector = connectors.begin(); connector != connectors.end(); connector++) {
        auto statusNotificationMsg = (*connector)->loop();
        if (statusNotificationMsg != nullptr) {
            auto statusNotification = makeOcppOperation(statusNotificationMsg);
            context.initiateOperation(std::move(statusNotification));
        }
    }
}

ConnectorStatus *ChargePointStatusService::getConnector(int connectorId) {
    if (connectorId < 0 || connectorId >= connectors.size()) {
        AO_DBG_ERR("connectorId is out of bounds");
        return nullptr;
    }

    return connectors.at(connectorId).get();
}

void ChargePointStatusService::boot() {
    booted = true;
}

bool ChargePointStatusService::isBooted() {
    return booted;
}

int ChargePointStatusService::getNumConnectors() {
    return connectors.size();
}
