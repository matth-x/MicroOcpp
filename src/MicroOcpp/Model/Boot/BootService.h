// matth-x/MicroOcpp
// Copyright Matthias Akstaller 2019 - 2024
// MIT License

#ifndef MO_BOOTSERVICE_H
#define MO_BOOTSERVICE_H

#include <MicroOcpp/Core/ConfigurationKeyValue.h>
#include <MicroOcpp/Core/FilesystemAdapter.h>
#include <MicroOcpp/Core/Memory.h>
#include <memory>

#define MO_BOOT_INTERVAL_DEFAULT 60

namespace MicroOcpp {

struct BootStats {
    uint16_t bootNr = 0;
    uint16_t lastBootSuccess = 0;

    uint16_t getBootFailureCount() {
        return bootNr - lastBootSuccess;
    }
};

enum class RegistrationStatus {
    Accepted,
    Pending,
    Rejected,
    UNDEFINED
};

RegistrationStatus deserializeRegistrationStatus(const char *serialized);

class Context;

class BootService : public MemoryManaged {
private:
    Context& context;
    std::shared_ptr<FilesystemAdapter> filesystem;

    unsigned long interval_s = MO_BOOT_INTERVAL_DEFAULT;
    unsigned long lastBootNotification = -1UL / 2;

    RegistrationStatus status = RegistrationStatus::Pending;
    
    String cpCredentials;

    std::shared_ptr<Configuration> preBootTransactionsBool;

    bool activatedModel = false;

    unsigned long firstExecutionTimestamp = 0;
    bool executedFirstTime = false;
    bool executedLongTime = false;

public:
    BootService(Context& context, std::shared_ptr<FilesystemAdapter> filesystem);

    void loop();

    void setChargePointCredentials(JsonObject credentials);
    void setChargePointCredentials(const char *credentials); //credentials: serialized BootNotification payload
    std::unique_ptr<JsonDoc> getChargePointCredentials();

    void notifyRegistrationStatus(RegistrationStatus status);
    void setRetryInterval(unsigned long interval);

    static bool loadBootStats(std::shared_ptr<FilesystemAdapter> filesystem, BootStats& out);
    static bool storeBootStats(std::shared_ptr<FilesystemAdapter> filesystem, BootStats bstats);
};

}

#endif
