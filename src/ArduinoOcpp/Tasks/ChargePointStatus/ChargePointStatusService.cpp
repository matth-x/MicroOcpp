// matth-x/ArduinoOcpp
// Copyright Matthias Akstaller 2019 - 2023
// MIT License

#include <ArduinoOcpp/Tasks/ChargePointStatus/ChargePointStatusService.h>
#include <ArduinoOcpp/Core/OcppEngine.h>
#include <ArduinoOcpp/SimpleOcppOperationFactory.h>
#include <ArduinoOcpp/Core/Configuration.h>

#include <ArduinoOcpp/Debug.h>

#include <memory>
#include <string.h>

#define RESET_DELAY 15000

using namespace ArduinoOcpp;

ChargePointStatusService::ChargePointStatusService(OcppEngine& context, unsigned int numConn)
      : context(context) {

    for (unsigned int i = 0; i < numConn; i++) {
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

    resetRetries = declareConfiguration<int>("ResetRetries", 2, CONFIGURATION_FN, true, true, false, false);
    
    /*
     * Further configuration keys which correspond to the Core profile
     */
    declareConfiguration<bool>("AuthorizeRemoteTxRequests",false,CONFIGURATION_VOLATILE,false,true,false,false);
    declareConfiguration<int>("GetConfigurationMaxKeys",30,CONFIGURATION_VOLATILE,false,true,false,false);
}

ChargePointStatusService::~ChargePointStatusService() {

}

void ChargePointStatusService::loop() {
    for (auto connector = connectors.begin(); connector != connectors.end(); connector++) {
        auto transactionMsg = (*connector)->loop();
        if (transactionMsg != nullptr) {
            auto transactionOp = makeOcppOperation(transactionMsg);
            transactionOp->setTimeout(std::unique_ptr<Timeout>(new SuppressedTimeout()));
            context.initiateOperation(std::move(transactionOp));
        }
    }

    if (outstandingResetRetries > 0 && ao_tick_ms() - t_resetRetry >= RESET_DELAY) {
        t_resetRetry = ao_tick_ms();
        outstandingResetRetries--;
        if (executeReset) {
            AO_DBG_INFO("Reset device");
            executeReset(isHardReset);
        } else {
            AO_DBG_ERR("No Reset function set! Abort");
            outstandingResetRetries = 0;
        }
        AO_DBG_ERR("Reset device failure. %s", outstandingResetRetries == 0 ? "Abort" : "Retry");

        if (outstandingResetRetries <= 0) {
            for (auto connector = connectors.begin(); connector != connectors.end(); connector++) {
                (*connector)->setAvailabilityVolatile(true);
            }
        }
    }
}

ConnectorStatus *ChargePointStatusService::getConnector(int connectorId) {
    if (connectorId < 0 || connectorId >= (int) connectors.size()) {
        AO_DBG_ERR("connectorId is out of bounds");
        return nullptr;
    }

    return connectors.at(connectorId).get();
}

int ChargePointStatusService::getNumConnectors() {
    return connectors.size();
}

void ChargePointStatusService::setPreReset(std::function<bool(bool)> preReset) {
    this->preReset = preReset;
}

std::function<bool(bool)> ChargePointStatusService::getPreReset() {
    return this->preReset;
}

void ChargePointStatusService::setExecuteReset(std::function<void(bool)> executeReset) {
    this->executeReset = executeReset;
}

std::function<void(bool)> ChargePointStatusService::getExecuteReset() {
    return this->executeReset;
}

void ChargePointStatusService::initiateReset(bool isHard) {
    isHardReset = isHard;
    outstandingResetRetries = 1 + *resetRetries; //one initial try + no. of retries
    if (outstandingResetRetries > 5) {
        AO_DBG_ERR("no. of reset trials exceeds 5");
        outstandingResetRetries = 5;
    }
    t_resetRetry = ao_tick_ms();

    for (auto connector = connectors.begin(); connector != connectors.end(); connector++) {
        (*connector)->setAvailabilityVolatile(false);
    }
}

#if AO_PLATFORM == AO_PLATFORM_ARDUINO && (defined(ESP32) || defined(ESP8266))
std::function<void(bool isHard)> ArduinoOcpp::makeDefaultResetFn() {
    return [] (bool isHard) {
        AO_DBG_DEBUG("Perform ESP reset");
        ESP.restart();
    };
}
#endif //AO_PLATFORM == AO_PLATFORM_ARDUINO && (defined(ESP32) || defined(ESP8266))
