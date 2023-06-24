// matth-x/ArduinoOcpp
// Copyright Matthias Akstaller 2019 - 2023
// MIT License

#include <ArduinoOcpp/Model/Reset/ResetService.h>
#include <ArduinoOcpp/Context.h>
#include <ArduinoOcpp/Model/Model.h>
#include <ArduinoOcpp/Core/SimpleRequestFactory.h>
#include <ArduinoOcpp/Core/Configuration.h>
#include <ArduinoOcpp/Operations/Reset.h>

#include <ArduinoOcpp/Operations/Authorize.h>
#include <ArduinoOcpp/Operations/StartTransaction.h>
#include <ArduinoOcpp/Operations/StatusNotification.h>
#include <ArduinoOcpp/Operations/StopTransaction.h>

#include <ArduinoOcpp/Debug.h>

#include <memory>
#include <string.h>

#define RESET_DELAY 15000

using namespace ArduinoOcpp;

ResetService::ResetService(Context& context)
      : context(context) {

    resetRetries = declareConfiguration<int>("ResetRetries", 2, CONFIGURATION_FN, true, true, false, false);

    context.getOperationRegistry().registerOperation("Reset", [&context] () {
        return new Ocpp16::Reset(context.getModel());});
}

ResetService::~ResetService() {

}

void ResetService::loop() {

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
            for (unsigned int cId = 0; cId < context.getModel().getNumConnectors(); cId++) {
                auto connector = context.getModel().getConnector(cId);
                connector->setAvailabilityVolatile(true);
            }

            ChargePointStatus cpStatus = ChargePointStatus::NOT_SET;
            if (context.getModel().getNumConnectors() > 0) {
                cpStatus = context.getModel().getConnector(0)->getStatus();
            }

            auto statusNotification = makeRequest(new Ocpp16::StatusNotification(
                        0,
                        cpStatus, //will be determined in StatusNotification::initiate
                        context.getModel().getClock().now(),
                        "ResetFailure"));
            statusNotification->setTimeout(60000);
            context.initiateRequest(std::move(statusNotification));
        }
    }
}

void ResetService::setPreReset(std::function<bool(bool)> preReset) {
    this->preReset = preReset;
}

std::function<bool(bool)> ResetService::getPreReset() {
    return this->preReset;
}

void ResetService::setExecuteReset(std::function<void(bool)> executeReset) {
    this->executeReset = executeReset;
}

std::function<void(bool)> ResetService::getExecuteReset() {
    return this->executeReset;
}

void ResetService::initiateReset(bool isHard) {
    isHardReset = isHard;
    outstandingResetRetries = 1 + *resetRetries; //one initial try + no. of retries
    if (outstandingResetRetries > 5) {
        AO_DBG_ERR("no. of reset trials exceeds 5");
        outstandingResetRetries = 5;
    }
    t_resetRetry = ao_tick_ms();

    for (unsigned int cId = 0; cId < context.getModel().getNumConnectors(); cId++) {
        auto connector = context.getModel().getConnector(cId);
        connector->setAvailabilityVolatile(false);
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
