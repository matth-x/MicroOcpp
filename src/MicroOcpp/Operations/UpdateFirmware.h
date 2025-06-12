// matth-x/MicroOcpp
// Copyright Matthias Akstaller 2019 - 2024
// MIT License

#ifndef MO_UPDATEFIRMWARE_H
#define MO_UPDATEFIRMWARE_H

#include <MicroOcpp/Core/Operation.h>
#include <MicroOcpp/Core/Time.h>
#include <MicroOcpp/Version.h>

#if MO_ENABLE_V16 && MO_ENABLE_FIRMWAREMANAGEMENT

namespace MicroOcpp {

class Context;

namespace Ocpp16 {

class FirmwareService;

class UpdateFirmware : public Operation, public MemoryManaged {
private:
    Context& context;
    FirmwareService& fwService;

    const char *errorCode = nullptr;
public:
    UpdateFirmware(Context& context, FirmwareService& fwService);

    const char* getOperationType() override {return "UpdateFirmware";}

    void processReq(JsonObject payload) override;

    std::unique_ptr<JsonDoc> createConf() override;

    const char *getErrorCode() override {return errorCode;}
};

} //namespace Ocpp16
} //namespace MicroOcpp
#endif //MO_ENABLE_V16 && MO_ENABLE_FIRMWAREMANAGEMENT
#endif
