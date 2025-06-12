// matth-x/MicroOcpp
// Copyright Matthias Akstaller 2019 - 2025
// MIT License

#ifndef MO_SECURITYEVENTNOTIFICATION_H
#define MO_SECURITYEVENTNOTIFICATION_H

#include <MicroOcpp/Core/Operation.h>
#include <MicroOcpp/Core/Memory.h>
#include <MicroOcpp/Core/Time.h>
#include <MicroOcpp/Model/SecurityEvent/SecurityEvent.h>
#include <MicroOcpp/Version.h>

#if (MO_ENABLE_V16 || MO_ENABLE_V201) && MO_ENABLE_SECURITY_EVENT

namespace MicroOcpp {

class SecurityEventNotification : public Operation, public MemoryManaged {
private:
    Context& context;
    char type [MO_SECURITY_EVENT_TYPE_LENGTH + 1];
    Timestamp timestamp;

    const char *errorCode = nullptr;
public:
    SecurityEventNotification(Context& context, const char *type, const Timestamp& timestamp); // the length of type, not counting the terminating 0, must be <= MO_SECURITY_EVENT_TYPE_LENGTH. Excess characters are cropped

    const char* getOperationType() override;

    std::unique_ptr<JsonDoc> createReq() override;

    void processConf(JsonObject payload) override;

    const char *getErrorCode() override {return errorCode;}

    void processReq(JsonObject payload) override;

    std::unique_ptr<JsonDoc> createConf() override;

};

} //namespace MicroOcpp
#endif //(MO_ENABLE_V16 || MO_ENABLE_V201) && MO_ENABLE_SECURITY_EVENT
#endif
