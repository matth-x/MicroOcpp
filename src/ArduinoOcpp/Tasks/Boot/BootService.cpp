// matth-x/ArduinoOcpp
// Copyright Matthias Akstaller 2019 - 2023
// MIT License

#include <ArduinoOcpp/Tasks/Boot/BootService.h>
#include <ArduinoOcpp/Core/OcppEngine.h>
#include <ArduinoOcpp/Core/OcppModel.h>
#include <ArduinoOcpp/SimpleOcppOperationFactory.h>
#include <ArduinoOcpp/MessagesV16/BootNotification.h>
#include <ArduinoOcpp/Tasks/ChargePointStatus/ChargePointStatusService.h>
#include <ArduinoOcpp/Platform.h>
#include <ArduinoOcpp/Debug.h>

using namespace ArduinoOcpp;

RegistrationStatus ArduinoOcpp::deserializeRegistrationStatus(const char *serialized) {
    if (!strcmp(serialized, "Accepted")) {
        return RegistrationStatus::Accepted;
    } else if (!strcmp(serialized, "Pending")) {
        return RegistrationStatus::Pending;
    } else if (!strcmp(serialized, "Rejected")) {
        return RegistrationStatus::Rejected;
    } else {
        AO_DBG_ERR("deserialization error");
        return RegistrationStatus::UNDEFINED;
    }
}

BootService::BootService(OcppEngine& context) : context(context) {
    
    //if transactions can start before the BootNotification succeeds
    preBootTransactions = declareConfiguration<bool>("AO_PreBootTransactions", false, CONFIGURATION_FN, true, true, true, false);
    
    if (!preBootTransactions) {
        AO_DBG_ERR("initialization error");
        (void)0;
    }
}

void BootService::loop() {

    if (!activatedPostBootCommunication && status == RegistrationStatus::Accepted) {
        context.activatePostBootCommunication();
        activatedPostBootCommunication = true;
    }

    if (!activatedModel && (status == RegistrationStatus::Accepted || *preBootTransactions)) {
        context.getOcppModel().activateTasks();
        activatedModel = true;
    }

    if (status == RegistrationStatus::Accepted) {
        return;
    }
    
    if (ao_tick_ms() - lastBootNotification < (interval_s * 1000UL)) {
        return;
    }

    /*
     * Create BootNotification. The BootNotifaction object will fetch its paremeters from
     * this class and notify this class about the response
     */
    auto bootNotification = makeOcppOperation(new Ocpp16::BootNotification());
    bootNotification->setTimeout(0);
    context.initiatePreBootOperation(std::move(bootNotification));

    lastBootNotification = ao_tick_ms();
}

void BootService::setChargePointCredentials(const char *credentialsJson) {
    cpCredentials = credentialsJson;
    if (cpCredentials.size() < 2) {
        cpCredentials = "{}";
    }
}

std::string& BootService::getChargePointCredentials() {
    if (cpCredentials.size() <= 2) {
        cpCredentials = "{}";
    }

    return cpCredentials;
}

void BootService::notifyRegistrationStatus(RegistrationStatus status) {
    this->status = status;
}

void BootService::setRetryInterval(unsigned long interval_s) {
    if (interval_s == 0) {
        this->interval_s = AO_BOOT_INTERVAL_DEFAULT;
    } else {
        this->interval_s = interval_s;
    }
}
