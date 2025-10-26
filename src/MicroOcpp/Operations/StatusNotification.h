// matth-x/MicroOcpp
// Copyright Matthias Akstaller 2019 - 2024
// MIT License

#ifndef MO_STATUSNOTIFICATION_H
#define MO_STATUSNOTIFICATION_H

#include <MicroOcpp/Core/Operation.h>
#include <MicroOcpp/Core/Time.h>
#include <MicroOcpp/Model/Availability/AvailabilityDefs.h>
#include <MicroOcpp/Model/Common/EvseId.h>
#include <MicroOcpp/Version.h>

#if MO_ENABLE_V16

namespace MicroOcpp {

class Context;

namespace v16 {

class StatusNotification : public Operation, public MemoryManaged {
private:
    Context& context;
    int connectorId = 1;
    MO_ChargePointStatus currentStatus = MO_ChargePointStatus_UNDEFINED;
    Timestamp timestamp;
    MO_ErrorData errorData;
public:
    StatusNotification(Context& context, int connectorId, MO_ChargePointStatus currentStatus, const Timestamp &timestamp, MO_ErrorData errorData);

    const char* getOperationType() override;

    std::unique_ptr<JsonDoc> createReq() override;

    void processConf(JsonObject payload) override;

    void processReq(JsonObject payload) override;

    std::unique_ptr<JsonDoc> createConf() override;
};

} //namespace v16
} //namespace MicroOcpp
#endif //MO_ENABLE_V16

#if MO_ENABLE_V201

namespace MicroOcpp {
namespace v201 {

class StatusNotification : public Operation, public MemoryManaged {
private:
    Context& context;
    EvseId evseId;
    Timestamp timestamp;
    MO_ChargePointStatus currentStatus = MO_ChargePointStatus_UNDEFINED;
public:
    StatusNotification(Context& context, EvseId evseId, MO_ChargePointStatus currentStatus, const Timestamp &timestamp);

    const char* getOperationType() override;

    std::unique_ptr<JsonDoc> createReq() override;

    void processConf(JsonObject payload) override;

    void processReq(JsonObject payload) override;

    std::unique_ptr<JsonDoc> createConf() override;
};

} //namespace v201
} //namespace MicroOcpp
#endif //MO_ENABLE_V201
#endif
