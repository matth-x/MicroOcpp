// matth-x/ArduinoOcpp
// Copyright Matthias Akstaller 2019 - 2023
// MIT License

#include <ArduinoOcpp/Tasks/Boot/BootService.h>
#include <ArduinoOcpp/Core/Context.h>
#include <ArduinoOcpp/Core/Model.h>
#include <ArduinoOcpp/Core/Configuration.h>
#include <ArduinoOcpp/Core/SimpleRequestFactory.h>
#include <ArduinoOcpp/MessagesV16/BootNotification.h>
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

BootService::BootService(Context& context) : context(context) {
    
    //if transactions can start before the BootNotification succeeds
    preBootTransactions = declareConfiguration<bool>("AO_PreBootTransactions", false, CONFIGURATION_FN, true, true, true, false);
    
    if (!preBootTransactions) {
        AO_DBG_ERR("initialization error");
        (void)0;
    }

    //Register message handler for TriggerMessage operation
    context.getOperationRegistry().registerOperation("BootNotification", [this] () {
        return new Ocpp16::BootNotification(this->context.getModel(), getChargePointCredentials());});
}

void BootService::loop() {

    if (!activatedPostBootCommunication && status == RegistrationStatus::Accepted) {
        context.activatePostBootCommunication();
        activatedPostBootCommunication = true;
    }

    if (!activatedModel && (status == RegistrationStatus::Accepted || *preBootTransactions)) {
        context.getModel().activateTasks();
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
    auto bootNotification = makeRequest(new Ocpp16::BootNotification(context.getModel(), getChargePointCredentials()));
    bootNotification->setTimeout(interval_s * 1000UL);
    context.initiatePreBootOperation(std::move(bootNotification));

    lastBootNotification = ao_tick_ms();
}

void BootService::setChargePointCredentials(JsonObject credentials) {
    auto written = serializeJson(credentials, cpCredentials);
    if (written < 2) {
        AO_DBG_ERR("serialization error");
        cpCredentials = "{}";
    }
}

void BootService::setChargePointCredentials(const char *credentials) {
    cpCredentials = credentials;
    if (cpCredentials.size() < 2) {
        cpCredentials = "{}";
    }
}

std::unique_ptr<DynamicJsonDocument> BootService::getChargePointCredentials() {
    if (cpCredentials.size() <= 2) {
        return createEmptyDocument();
    }

    std::unique_ptr<DynamicJsonDocument> doc;
    size_t capacity = JSON_OBJECT_SIZE(9) + cpCredentials.size();
    DeserializationError err = DeserializationError::NoMemory;
    while (err == DeserializationError::NoMemory && capacity <= AO_MAX_JSON_CAPACITY) {
        doc.reset(new DynamicJsonDocument(capacity));
        err = deserializeJson(*doc, cpCredentials);

        capacity *= 2;
    }

    if (!err) {
        return doc;
    } else {
        AO_DBG_ERR("could not parse stored credentials: %s", err.c_str());
        return nullptr;
    }
}

void BootService::notifyRegistrationStatus(RegistrationStatus status) {
    this->status = status;
    lastBootNotification = ao_tick_ms();
}

void BootService::setRetryInterval(unsigned long interval_s) {
    if (interval_s == 0) {
        this->interval_s = AO_BOOT_INTERVAL_DEFAULT;
    } else {
        this->interval_s = interval_s;
    }
    lastBootNotification = ao_tick_ms();
}
