// matth-x/MicroOcpp
// Copyright Matthias Akstaller 2019 - 2023
// MIT License

#ifndef UPDATEFIRMWARE_H
#define UPDATEFIRMWARE_H

#include <MicroOcpp/Core/Operation.h>
#include <MicroOcpp/Core/Time.h>

namespace MicroOcpp {

class FirmwareService;

namespace Ocpp16 {

class UpdateFirmware : public Operation {
private:
  FirmwareService& fwService;

  const char *errorCode = nullptr;
public:
  UpdateFirmware(FirmwareService& fwService);

  const char* getOperationType() override {return "UpdateFirmware";}

  void processReq(JsonObject payload) override;

  std::unique_ptr<DynamicJsonDocument> createConf() override;

  const char *getErrorCode() override {return errorCode;}
};

} //end namespace Ocpp16
} //end namespace MicroOcpp

#endif
