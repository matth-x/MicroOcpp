// matth-x/MicroOcpp
// Copyright Matthias Akstaller 2019 - 2024
// MIT License

#ifndef MO_FIRMWARESTATUSNOTIFICATION_H
#define MO_FIRMWARESTATUSNOTIFICATION_H

#include <MicroOcpp/Core/Operation.h>
#include <MicroOcpp/Model/FirmwareManagement/FirmwareStatus.h>
#include <MicroOcpp/Version.h>

#if MO_ENABLE_V16 && MO_ENABLE_FIRMWAREMANAGEMENT

namespace MicroOcpp {
namespace Ocpp16 {

class FirmwareStatusNotification : public Operation, public MemoryManaged {
private:
    FirmwareStatus status = FirmwareStatus::Idle;
    static const char *cstrFromFwStatus(FirmwareStatus status);
public:
    FirmwareStatusNotification(FirmwareStatus status);

    const char* getOperationType() override {return "FirmwareStatusNotification"; }

    std::unique_ptr<JsonDoc> createReq() override;

    void processConf(JsonObject payload) override;
};

} //namespace Ocpp16
} //namespace MicroOcpp
#endif //MO_ENABLE_V16 && MO_ENABLE_FIRMWAREMANAGEMENT
#endif
