// matth-x/MicroOcpp
// Copyright Matthias Akstaller 2019 - 2024
// MIT License

#include <MicroOcpp/Core/Operation.h>

#include <MicroOcpp/Model/FirmwareManagement/FirmwareStatus.h>

#ifndef MO_FIRMWARESTATUSNOTIFICATION_H
#define MO_FIRMWARESTATUSNOTIFICATION_H

namespace MicroOcpp {
namespace Ocpp16 {

class FirmwareStatusNotification : public Operation {
private:
    FirmwareStatus status = FirmwareStatus::Idle;
    static const char *cstrFromFwStatus(FirmwareStatus status);
public:
    FirmwareStatusNotification(FirmwareStatus status);

    const char* getOperationType() override {return "FirmwareStatusNotification"; }

    std::unique_ptr<DynamicJsonDocument> createReq() override;

    void processConf(JsonObject payload) override;

};

} //end namespace Ocpp16
} //end namespace MicroOcpp

#endif
