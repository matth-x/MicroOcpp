// matth-x/MicroOcpp
// Copyright Matthias Akstaller 2019 - 2024
// MIT License

#ifndef MO_SECURITYEVENTNOTIFICATION_H
#define MO_SECURITYEVENTNOTIFICATION_H

#include <MicroOcpp/Version.h>

#if MO_ENABLE_V201

#include <MicroOcpp/Core/Operation.h>
#include <MicroOcpp/Core/Memory.h>
#include <MicroOcpp/Core/Time.h>

namespace MicroOcpp {

namespace Ocpp201 {

class SecurityEventNotification : public Operation, public MemoryManaged {
private:
    String type;
    Timestamp timestamp;

    const char *errorCode = nullptr;
public:
    SecurityEventNotification(const char *type, const Timestamp& timestamp);

    const char* getOperationType() override;

    std::unique_ptr<JsonDoc> createReq() override;

    void processConf(JsonObject payload) override;

    const char *getErrorCode() override {return errorCode;}

    void processReq(JsonObject payload) override;

    std::unique_ptr<JsonDoc> createConf() override;

};

} //namespace Ocpp201
} //namespace MicroOcpp

#endif //MO_ENABLE_V201

#endif
