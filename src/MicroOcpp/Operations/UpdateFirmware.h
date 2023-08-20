// matth-x/MicroOcpp
// Copyright Matthias Akstaller 2019 - 2023
// MIT License

#ifndef UPDATEFIRMWARE_H
#define UPDATEFIRMWARE_H

#include <MicroOcpp/Core/Operation.h>
#include <MicroOcpp/Core/Time.h>

namespace MicroOcpp {

class Model;

namespace Ocpp16 {

class UpdateFirmware : public Operation {
private:
  Model& model;
  std::string location {};
  Timestamp retreiveDate = Timestamp();
  int retries = 1;
  unsigned long retryInterval = 180;
  bool formatError = false;
public:
  UpdateFirmware(Model& model);

  const char* getOperationType() {return "UpdateFirmware";}

  void processReq(JsonObject payload);

  std::unique_ptr<DynamicJsonDocument> createConf();

  const char *getErrorCode() {if (formatError) return "FormationViolation"; else return NULL;}
};

} //end namespace Ocpp16
} //end namespace MicroOcpp

#endif
