// matth-x/MicroOcpp
// Copyright Matthias Akstaller 2019 - 2024
// MIT License

#ifndef STATUSNOTIFICATION_H
#define STATUSNOTIFICATION_H

#include <MicroOcpp/Core/Operation.h>
#include <MicroOcpp/Core/Time.h>
#include <MicroOcpp/Model/ConnectorBase/ChargePointStatus.h>
#include <MicroOcpp/Model/ConnectorBase/ChargePointErrorData.h>
#include <MicroOcpp/Version.h>

namespace MicroOcpp {
    
const char *cstrFromOcppEveState(ChargePointStatus state);

namespace Ocpp16 {

class StatusNotification : public Operation, public MemoryManaged {
private:
    int connectorId = 1;
    ChargePointStatus currentStatus = ChargePointStatus_UNDEFINED;
    Timestamp timestamp;
    ErrorData errorData;
public:
    StatusNotification(int connectorId, ChargePointStatus currentStatus, const Timestamp &timestamp, ErrorData errorData = nullptr);

    const char* getOperationType() override;

    std::unique_ptr<JsonDoc> createReq() override;

    void processConf(JsonObject payload) override;

    void processReq(JsonObject payload) override;

    std::unique_ptr<JsonDoc> createConf() override;

    int getConnectorId() {
        return connectorId;
    }
};

} // namespace Ocpp16
} // namespace MicroOcpp

#if MO_ENABLE_V201

#include <MicroOcpp/Model/ConnectorBase/EvseId.h>

namespace MicroOcpp {
namespace Ocpp201 {

class StatusNotification : public Operation, public MemoryManaged {
private:
    EvseId evseId;
    Timestamp timestamp;
    ChargePointStatus currentStatus = ChargePointStatus_UNDEFINED;
public:
    StatusNotification(EvseId evseId, ChargePointStatus currentStatus, const Timestamp &timestamp);

    const char* getOperationType() override;

    std::unique_ptr<JsonDoc> createReq() override;

    void processConf(JsonObject payload) override;
};

} // namespace Ocpp201
} // namespace MicroOcpp

#endif //MO_ENABLE_V201

#endif
