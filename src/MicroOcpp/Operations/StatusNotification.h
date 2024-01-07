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
namespace Ocpp16 {

class StatusNotification : public Operation {
private:
    int connectorId = 1;
    ChargePointStatus currentStatus = ChargePointStatus::NOT_SET;
    Timestamp timestamp;
    ErrorData errorData;
public:
    StatusNotification(int connectorId, ChargePointStatus currentStatus, const Timestamp &timestamp, ErrorData errorData = nullptr);

    const char* getOperationType() override;

    std::unique_ptr<DynamicJsonDocument> createReq() override;

    void processConf(JsonObject payload) override;

    void processReq(JsonObject payload) override;

    std::unique_ptr<DynamicJsonDocument> createConf() override;
};

const char *cstrFromOcppEveState(ChargePointStatus state);

} //end namespace Ocpp16

#if MO_ENABLE_V201

namespace Ocpp201 {

class StatusNotification : public Operation {
private:
    Timestamp timestamp;
    ConnectorStatus currentStatus = ConnectorStatus::NOT_SET;
    int evseId;
    int connectorId;
public:
    StatusNotification(int evseId, ConnectorStatus currentStatus, const Timestamp &timestamp, int connectorId);

    const char* getOperationType() override;

    std::unique_ptr<DynamicJsonDocument> createReq() override;

    void processConf(JsonObject payload) override;
};

const char *cstrFromOcppEveState(ConnectorStatus state);

} //end namespace Ocpp201

#endif //MO_ENABLE_V201

} //end namespace MicroOcpp
#endif
