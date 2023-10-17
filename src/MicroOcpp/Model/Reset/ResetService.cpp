// matth-x/MicroOcpp
// Copyright Matthias Akstaller 2019 - 2023
// MIT License

#include <MicroOcpp/Model/Reset/ResetService.h>
#include <MicroOcpp/Core/Context.h>
#include <MicroOcpp/Model/Model.h>
#include <MicroOcpp/Core/SimpleRequestFactory.h>
#include <MicroOcpp/Core/Configuration.h>
#include <MicroOcpp/Operations/Reset.h>

#include <MicroOcpp/Operations/Authorize.h>
#include <MicroOcpp/Operations/StartTransaction.h>
#include <MicroOcpp/Operations/StatusNotification.h>
#include <MicroOcpp/Operations/StopTransaction.h>

#include <MicroOcpp/Debug.h>

#include <memory>
#include <string.h>

#define RESET_DELAY 15000

using namespace MicroOcpp;

ResetService::ResetService(Context& context)
      : context(context) {

    resetRetriesInt = declareConfiguration<int>("ResetRetries", 2);

    context.getOperationRegistry().registerOperation("Reset", [&context] () {
        return new Ocpp16::Reset(context.getModel());});
}

ResetService::~ResetService() {

}

void ResetService::loop() {

    if (outstandingResetRetries > 0 && mocpp_tick_ms() - t_resetRetry >= RESET_DELAY) {
        t_resetRetry = mocpp_tick_ms();
        outstandingResetRetries--;
        if (executeReset) {
            MO_DBG_INFO("Reset device");
            executeReset(isHardReset);
        } else {
            MO_DBG_ERR("No Reset function set! Abort");
            outstandingResetRetries = 0;
        }
        MO_DBG_ERR("Reset device failure. %s", outstandingResetRetries == 0 ? "Abort" : "Retry");

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
    outstandingResetRetries = 1 + resetRetriesInt->getInt(); //one initial try + no. of retries
    if (outstandingResetRetries > 5) {
        MO_DBG_ERR("no. of reset trials exceeds 5");
        outstandingResetRetries = 5;
    }
    t_resetRetry = mocpp_tick_ms();

    for (unsigned int cId = 0; cId < context.getModel().getNumConnectors(); cId++) {
        auto connector = context.getModel().getConnector(cId);
        connector->setAvailabilityVolatile(false);
    }
}

#if MO_PLATFORM == MO_PLATFORM_ARDUINO && (defined(ESP32) || defined(ESP8266))
std::function<void(bool isHard)> MicroOcpp::makeDefaultResetFn() {
    return [] (bool isHard) {
        MO_DBG_DEBUG("Perform ESP reset");
        ESP.restart();
    };
}
#endif //MO_PLATFORM == MO_PLATFORM_ARDUINO && (defined(ESP32) || defined(ESP8266))
