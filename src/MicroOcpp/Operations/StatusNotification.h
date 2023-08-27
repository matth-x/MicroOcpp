// matth-x/MicroOcpp
// Copyright Matthias Akstaller 2019 - 2023
// MIT License

#ifndef STATUSNOTIFICATION_H
#define STATUSNOTIFICATION_H

#include <MicroOcpp/Core/Operation.h>
#include <MicroOcpp/Core/Time.h>
#include <MicroOcpp/Model/ConnectorBase/ChargePointStatus.h>
#include <MicroOcpp/Model/ConnectorBase/ChargePointErrorData.h>

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
} //end namespace MicroOcpp
#endif
