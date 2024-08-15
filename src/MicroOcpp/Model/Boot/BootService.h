// matth-x/MicroOcpp
// Copyright Matthias Akstaller 2019 - 2024
// MIT License

#ifndef MO_BOOTSERVICE_H
#define MO_BOOTSERVICE_H

#include <MicroOcpp/Core/ConfigurationKeyValue.h>
#include <MicroOcpp/Core/FilesystemAdapter.h>
#include <MicroOcpp/Core/RequestQueue.h>
#include <MicroOcpp/Core/Memory.h>
#include <memory>

#define MO_BOOT_INTERVAL_DEFAULT 60

#ifndef MO_BOOTSTATS_LONGTIME_MS
#define MO_BOOTSTATS_LONGTIME_MS 180 * 1000
#endif

namespace MicroOcpp {

#define MO_BOOTSTATS_VERSION_SIZE 10

struct BootStats {
    uint16_t bootNr = 0;
    uint16_t lastBootSuccess = 0;

    uint16_t getBootFailureCount() {
        return bootNr - lastBootSuccess;
    }

    char microOcppVersion [MO_BOOTSTATS_VERSION_SIZE] = {'\0'};
};

enum class RegistrationStatus {
    Accepted,
    Pending,
    Rejected,
    UNDEFINED
};

RegistrationStatus deserializeRegistrationStatus(const char *serialized);

class PreBootQueue : public VolatileRequestQueue {
private:
    bool activatedPostBootCommunication = false;
public:
    unsigned int getFrontRequestOpNr() override; //override FrontRequestOpNr behavior: in PreBoot mode, always return 0 to avoid other RequestEmitters from sending msgs
    
    void activatePostBootCommunication(); //end PreBoot mode, now send Requests normally
};

class Context;

class BootService : public MemoryManaged {
private:
    Context& context;
    std::shared_ptr<FilesystemAdapter> filesystem;

    PreBootQueue preBootQueue;

    unsigned long interval_s = MO_BOOT_INTERVAL_DEFAULT;
    unsigned long lastBootNotification = -1UL / 2;

    RegistrationStatus status = RegistrationStatus::Pending;
    
    String cpCredentials;

    std::shared_ptr<Configuration> preBootTransactionsBool;

    bool activatedModel = false;
    bool activatedPostBootCommunication = false;

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

    static bool loadBootStats(std::shared_ptr<FilesystemAdapter> filesystem, BootStats& bstats);
    static bool storeBootStats(std::shared_ptr<FilesystemAdapter> filesystem, BootStats& bstats);

    static bool recover(std::shared_ptr<FilesystemAdapter> filesystem, BootStats& bstats); //delete all persistent files which could lead to a crash

    static bool migrate(std::shared_ptr<FilesystemAdapter> filesystem, BootStats& bstats); //migrate persistent storage if running on a new MO version
};

}

#endif
