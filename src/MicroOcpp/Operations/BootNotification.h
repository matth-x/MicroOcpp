// matth-x/MicroOcpp
// Copyright Matthias Akstaller 2019 - 2024
// MIT License

#ifndef MO_BOOTNOTIFICATION_H
#define MO_BOOTNOTIFICATION_H

#include <MicroOcpp/Model/Boot/BootNotificationData.h>
#include <MicroOcpp/Core/Operation.h>
#include <MicroOcpp/Version.h>

#if MO_ENABLE_V16 || MO_ENABLE_V201

namespace MicroOcpp {

class Context;
class BootService;
class HeartbeatService;

class BootNotification : public Operation, public MemoryManaged {
private:
    Context& context;
    BootService& bootService;
    HeartbeatService *heartbeatService = nullptr;
    const MO_BootNotificationData& bnData;
    const char *reason201 = nullptr;
    int ocppVersion = -1;
    const char *errorCode = nullptr;
public:
    BootNotification(Context& context, BootService& bootService, HeartbeatService *heartbeatService, const MO_BootNotificationData& bnData, const char *reason201);

    ~BootNotification() = default;

    const char* getOperationType() override;

    std::unique_ptr<JsonDoc> createReq() override;

    void processConf(JsonObject payload) override;

    const char *getErrorCode() override {return errorCode;}

#if MO_ENABLE_MOCK_SERVER
    static int writeMockConf(const char *operationType, char *buf, size_t size, void *userStatus, void *userData);
#endif
};

} //namespace MicroOcpp

#endif //MO_ENABLE_V16 || MO_ENABLE_V201
#endif
