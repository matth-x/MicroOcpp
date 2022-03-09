// matth-x/ArduinoOcpp
// Copyright Matthias Akstaller 2019 - 2022
// MIT License

#include <ArduinoOcpp/Tasks/ChargePointStatus/ChargePointStatusService.h>
#include <ArduinoOcpp/Core/OcppEngine.h>
#include <ArduinoOcpp/SimpleOcppOperationFactory.h>
#include <ArduinoOcpp/Core/Configuration.h>

#include <ArduinoOcpp/Debug.h>

#include <string.h>

using namespace ArduinoOcpp;

ChargePointStatusService::ChargePointStatusService(OcppEngine& context, unsigned int numConn)
      : context(context) {

    for (int i = 0; i < numConn; i++) {
        connectors.push_back(std::unique_ptr<ConnectorStatus>(new ConnectorStatus(context.getOcppModel(), i)));
    }

    
    std::shared_ptr<Configuration<int>> numberOfConnectors =
            declareConfiguration<int>("NumberOfConnectors", numConn >= 1 ? numConn - 1 : 0, CONFIGURATION_VOLATILE, false, true, false, false);

    const char *fpId = "Core,RemoteTrigger";
    const char *fpIdCore = "Core";
    const char *fpIdRTrigger = "RemoteTrigger";
    auto fProfile = declareConfiguration<const char*>("SupportedFeatureProfiles",fpId, CONFIGURATION_VOLATILE, false, true, true, false);
    if (!strstr(*fProfile, fpIdCore)) {
        auto fProfilePlus = std::string(*fProfile);
        if (!fProfilePlus.empty() && fProfilePlus.back() != ',')
            fProfilePlus += ",";
        fProfilePlus += fpIdCore;
        fProfile->setValue(fProfilePlus.c_str(), fProfilePlus.length() + 1);
    }
    if (!strstr(*fProfile, fpIdRTrigger)) {
        auto fProfilePlus = std::string(*fProfile);
        if (!fProfilePlus.empty() && fProfilePlus.back() != ',')
            fProfilePlus += ",";
        fProfilePlus += fpIdRTrigger;
        fProfile->setValue(fProfilePlus.c_str(), fProfilePlus.length() + 1);
    }

    /*
     * Further configuration keys which correspond to the Core profile
     */
    declareConfiguration<const char*>("AuthorizeRemoteTxRequests","false",CONFIGURATION_VOLATILE,false,true,false,false);
    declareConfiguration<int>("GetConfigurationMaxKeys",30,CONFIGURATION_VOLATILE,false,true,false,false);
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
