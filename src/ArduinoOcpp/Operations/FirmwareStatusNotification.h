// matth-x/ArduinoOcpp
// Copyright Matthias Akstaller 2019 - 2023
// MIT License

#include <ArduinoOcpp/Core/Operation.h>

#include <ArduinoOcpp/Tasks/FirmwareManagement/FirmwareStatus.h>

#ifndef FIRMWARESTATUSNOTIFICATION_H
#define FIRMWARESTATUSNOTIFICATION_H

namespace ArduinoOcpp {
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
} //end namespace ArduinoOcpp

#endif
