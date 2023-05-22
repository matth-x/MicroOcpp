// matth-x/ArduinoOcpp
// Copyright Matthias Akstaller 2019 - 2023
// MIT License

#include <ArduinoOcpp/Tasks/ChargeControl/ChargeControlService.h>
#include <ArduinoOcpp/Core/OcppEngine.h>
#include <ArduinoOcpp/SimpleOcppOperationFactory.h>
#include <ArduinoOcpp/Core/Configuration.h>
#include <ArduinoOcpp/MessagesV16/ChangeAvailability.h>
#include <ArduinoOcpp/MessagesV16/ChangeConfiguration.h>
#include <ArduinoOcpp/MessagesV16/ClearCache.h>
#include <ArduinoOcpp/MessagesV16/GetConfiguration.h>
#include <ArduinoOcpp/MessagesV16/RemoteStartTransaction.h>
#include <ArduinoOcpp/MessagesV16/RemoteStopTransaction.h>
#include <ArduinoOcpp/MessagesV16/Reset.h>
#include <ArduinoOcpp/MessagesV16/TriggerMessage.h>
#include <ArduinoOcpp/MessagesV16/UnlockConnector.h>

#include <ArduinoOcpp/MessagesV16/Authorize.h>
#include <ArduinoOcpp/MessagesV16/StartTransaction.h>
#include <ArduinoOcpp/MessagesV16/StatusNotification.h>
#include <ArduinoOcpp/MessagesV16/StopTransaction.h>

#include <ArduinoOcpp/Debug.h>

#include <memory>
#include <string.h>

#define RESET_DELAY 15000

using namespace ArduinoOcpp;

ChargeControlService::ChargeControlService(OcppEngine& context, unsigned int numConn, std::shared_ptr<FilesystemAdapter> filesystem)
      : context(context) {

    for (unsigned int i = 0; i < numConn; i++) {
        connectors.push_back(std::unique_ptr<Connector>(new Connector(context, i)));
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

    
    context.getOperationDeserializer().registerOcppOperation("ChangeAvailability", [&context] () {
        return new Ocpp16::ChangeAvailability(context.getOcppModel());});
    context.getOperationDeserializer().registerOcppOperation("ChangeConfiguration", [] () {
        return new Ocpp16::ChangeConfiguration();});
    context.getOperationDeserializer().registerOcppOperation("ClearCache", [filesystem] () {
        return new Ocpp16::ClearCache(filesystem);});
    context.getOperationDeserializer().registerOcppOperation("GetConfiguration", [] () {
        return new Ocpp16::GetConfiguration();});
    context.getOperationDeserializer().registerOcppOperation("RemoteStartTransaction", [&context] () {
        return new Ocpp16::RemoteStartTransaction(context.getOcppModel());});
    context.getOperationDeserializer().registerOcppOperation("RemoteStopTransaction", [&context] () {
        return new Ocpp16::RemoteStopTransaction(context.getOcppModel());});
    context.getOperationDeserializer().registerOcppOperation("Reset", [&context] () {
        return new Ocpp16::Reset(context.getOcppModel());});
    context.getOperationDeserializer().registerOcppOperation("TriggerMessage", [&context] () {
        return new Ocpp16::TriggerMessage(context.getOcppModel());});
    context.getOperationDeserializer().registerOcppOperation("UnlockConnector", [&context] () {
        return new Ocpp16::UnlockConnector(context.getOcppModel());});

    /*
     * Register further message handlers to support echo mode: when this library
     * is connected with a WebSocket echo server, let it reply to its own requests.
     * Mocking an OCPP Server on the same device makes running (unit) tests easier.
     */
    context.getOperationDeserializer().registerOcppOperation("Authorize", [&context] () {
        return new Ocpp16::Authorize(context.getOcppModel(), nullptr);});
    context.getOperationDeserializer().registerOcppOperation("StartTransaction", [&context] () {
        return new Ocpp16::StartTransaction(context.getOcppModel(), nullptr);});
    context.getOperationDeserializer().registerOcppOperation("StatusNotification", [&context] () {
        return new Ocpp16::StatusNotification(-1, OcppEvseState::NOT_SET, OcppTimestamp());});
    context.getOperationDeserializer().registerOcppOperation("StopTransaction", [&context] () {
        return new Ocpp16::StopTransaction(context.getOcppModel(), nullptr);});
}

ChargeControlService::~ChargeControlService() {

}

void ChargeControlService::loop() {
    for (auto connector = connectors.begin(); connector != connectors.end(); connector++) {
        auto transactionMsg = (*connector)->loop();
        if (transactionMsg != nullptr) {
            auto transactionOp = makeOcppOperation(transactionMsg);
            transactionOp->setTimeout(0);
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

Connector *ChargeControlService::getConnector(int connectorId) {
    if (connectorId < 0 || connectorId >= (int) connectors.size()) {
        AO_DBG_ERR("connectorId is out of bounds");
        return nullptr;
    }

    return connectors.at(connectorId).get();
}

int ChargeControlService::getNumConnectors() {
    return connectors.size();
}

void ChargeControlService::setPreReset(std::function<bool(bool)> preReset) {
    this->preReset = preReset;
}

std::function<bool(bool)> ChargeControlService::getPreReset() {
    return this->preReset;
}

void ChargeControlService::setExecuteReset(std::function<void(bool)> executeReset) {
    this->executeReset = executeReset;
}

std::function<void(bool)> ChargeControlService::getExecuteReset() {
    return this->executeReset;
}

void ChargeControlService::initiateReset(bool isHard) {
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
