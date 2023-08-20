// matth-x/MicroOcpp
// Copyright Matthias Akstaller 2019 - 2023
// MIT License

#include <MicroOcpp/Core/Operation.h>

#include <MicroOcpp/Model/FirmwareManagement/FirmwareStatus.h>

#ifndef FIRMWARESTATUSNOTIFICATION_H
#define FIRMWARESTATUSNOTIFICATION_H

namespace MicroOcpp {
namespace Ocpp16 {

class FirmwareStatusNotification : public Operation {
private:
    FirmwareStatus status = FirmwareStatus::Idle;
    static const char *cstrFromFwStatus(FirmwareStatus status);
public:
    FirmwareStatusNotification(FirmwareStatus status);

    const char* getOperationType() {return "FirmwareStatusNotification"; }

    std::unique_ptr<DynamicJsonDocument> createReq();

    void processConf(JsonObject payload);

};

} //end namespace Ocpp16
} //end namespace MicroOcpp

#endif
