// matth-x/ArduinoOcpp
// Copyright Matthias Akstaller 2019 - 2023
// MIT License

#include <limits>

#include <ArduinoOcpp/Model/Boot/BootService.h>
#include <ArduinoOcpp/Core/Context.h>
#include <ArduinoOcpp/Model/Model.h>
#include <ArduinoOcpp/Core/Configuration.h>
#include <ArduinoOcpp/Core/SimpleRequestFactory.h>
#include <ArduinoOcpp/Core/FilesystemUtils.h>
#include <ArduinoOcpp/Operations/BootNotification.h>
#include <ArduinoOcpp/Platform.h>
#include <ArduinoOcpp/Debug.h>

#ifndef AO_BOOTSTATS_LONGTIME_MS
#define AO_BOOTSTATS_LONGTIME_MS 180 * 1000
#endif

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

BootService::BootService(Context& context, std::shared_ptr<FilesystemAdapter> filesystem) : context(context), filesystem(filesystem) {
    
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

    if (!executedFirstTime) {
        executedFirstTime = true;
        firstExecutionTimestamp = ao_tick_ms();
    }

    if (!executedLongTime && ao_tick_ms() - firstExecutionTimestamp >= AO_BOOTSTATS_LONGTIME_MS) {
        executedLongTime = true;
        AO_DBG_DEBUG("boot success timer reached");
        BootStats bootstats;
        loadBootStats(filesystem, bootstats);
        bootstats.lastBootSuccess = bootstats.bootNr;
        storeBootStats(filesystem, bootstats);
    }

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

bool BootService::loadBootStats(std::shared_ptr<FilesystemAdapter> filesystem, BootStats& bstats) {
    if (!filesystem) {
        return false;
    }

    size_t msize = 0;
    if (filesystem->stat(AO_FILENAME_PREFIX "bootstats.jsn", &msize) == 0) {
        
        bool success = true;

        auto json = FilesystemUtils::loadJson(filesystem, AO_FILENAME_PREFIX "bootstats.jsn");
        if (json) {
            int bootNrIn = (*json)["bootNr"] | -1;
            if (bootNrIn >= 0 && bootNrIn <= std::numeric_limits<uint16_t>::max()) {
                bstats.bootNr = (uint16_t) bootNrIn;
            } else {
                success = false;
            }

            int lastSuccessIn = (*json)["lastSuccess"] | -1;
            if (lastSuccessIn >= 0 && lastSuccessIn <= std::numeric_limits<uint16_t>::max()) {
                bstats.lastBootSuccess = (uint16_t) lastSuccessIn;
            } else {
                success = false;
            }
        } else {
            success = false;
        }

        if (!success) {
            AO_DBG_ERR("bootstats corrupted");
            filesystem->remove(AO_FILENAME_PREFIX "bootstats.jsn");
            bstats = BootStats();
        }

        return success;
    } else {
        return false;
    }
}

bool BootService::storeBootStats(std::shared_ptr<FilesystemAdapter> filesystem, BootStats bstats) {
    if (!filesystem) {
        return false;
    }

    DynamicJsonDocument json {JSON_OBJECT_SIZE(2)};

    json["bootNr"] = bstats.bootNr;
    json["lastSuccess"] = bstats.lastBootSuccess;

    return FilesystemUtils::storeJson(filesystem, AO_FILENAME_PREFIX "bootstats.jsn", json);
}
