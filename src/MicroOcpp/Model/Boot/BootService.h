// matth-x/MicroOcpp
// Copyright Matthias Akstaller 2019 - 2025
// MIT License

#ifndef MO_BOOTSERVICE_H
#define MO_BOOTSERVICE_H

#include <MicroOcpp/Model/Boot/BootNotificationData.h>
#include <MicroOcpp/Core/FilesystemAdapter.h>
#include <MicroOcpp/Core/RequestQueue.h>
#include <MicroOcpp/Core/Memory.h>
#include <MicroOcpp/Core/Time.h>
#include <MicroOcpp/Version.h>
#include <memory>

#if MO_ENABLE_V16 || MO_ENABLE_V201

#define MO_BOOT_INTERVAL_DEFAULT 60

#ifndef MO_BOOTSTATS_LONGTIME_MS
#define MO_BOOTSTATS_LONGTIME_MS 180 * 1000
#endif

namespace MicroOcpp {

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
    unsigned int getFrontRequestOpNr() override; //override FrontRequestOpNr behavior: in PreBoot mode, always return 0 to avoid other RequestQueues from sending msgs
    
    void activatePostBootCommunication(); //end PreBoot mode, now send Requests normally
};

class Context;
class HeartbeatService;

#if MO_ENABLE_V16
namespace Ocpp16 {
class Configuration;
}
#endif
#if MO_ENABLE_V201
namespace Ocpp201 {
class Variable;
}
#endif

class BootService : public MemoryManaged {
private:
    Context& context;
    MO_FilesystemAdapter *filesystem = nullptr;;

    PreBootQueue preBootQueue;

    HeartbeatService *heartbeatService = nullptr;

    int32_t interval_s = MO_BOOT_INTERVAL_DEFAULT;
    Timestamp lastBootNotification;

    RegistrationStatus status = RegistrationStatus::Pending;

    MO_BootNotificationData bnData {0};
    char *bnDataBuf = nullptr;

    int ocppVersion = -1;
    
    #if MO_ENABLE_V16
    Ocpp16::Configuration *preBootTransactionsBoolV16 = nullptr;
    #endif
    #if MO_ENABLE_V201
    Ocpp201::Variable *preBootTransactionsBoolV201 = nullptr;
    #endif

    bool activatedModel = false;
    bool activatedPostBootCommunication = false;

    Timestamp firstExecutionTimestamp;
    bool executedFirstTime = false;
    bool executedLongTime = false;

public:
    BootService(Context& context);
    ~BootService();

    bool setup();

    void loop();

    bool setBootNotificationData(MO_BootNotificationData bnData);
    const MO_BootNotificationData& getBootNotificationData();

    void notifyRegistrationStatus(RegistrationStatus status);
    RegistrationStatus getRegistrationStatus();

    void setRetryInterval(unsigned long interval);
};

}

#endif //MO_ENABLE_V16 || MO_ENABLE_V201
#endif
