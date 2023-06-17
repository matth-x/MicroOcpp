// matth-x/ArduinoOcpp
// Copyright Matthias Akstaller 2019 - 2023
// MIT License

#ifndef BOOTSERVICE_H
#define BOOTSERVICE_H

#include <ArduinoOcpp/Core/ConfigurationKeyValue.h>
#include <memory>

#define AO_BOOT_INTERVAL_DEFAULT 60

namespace ArduinoOcpp {

enum class RegistrationStatus {
    Accepted,
    Pending,
    Rejected,
    UNDEFINED
};

RegistrationStatus deserializeRegistrationStatus(const char *serialized);

class Context;

class BootService {
private:
    Context& context;

    unsigned long interval_s = AO_BOOT_INTERVAL_DEFAULT;
    unsigned long lastBootNotification = -1UL / 2;

    RegistrationStatus status = RegistrationStatus::Pending;
    
    std::string cpCredentials;

    std::shared_ptr<Configuration<bool>> preBootTransactions;

    bool activatedModel = false;
    bool activatedPostBootCommunication = false;

public:
    BootService(Context& context);

    void loop();

    void setChargePointCredentials(JsonObject credentials);
    void setChargePointCredentials(const char *credentials); //credentials: serialized BootNotification payload
    std::unique_ptr<DynamicJsonDocument> getChargePointCredentials();

    void notifyRegistrationStatus(RegistrationStatus status);
    void setRetryInterval(unsigned long interval);
};

}

#endif
