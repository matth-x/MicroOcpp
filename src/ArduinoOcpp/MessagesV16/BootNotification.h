// matth-x/ArduinoOcpp
// Copyright Matthias Akstaller 2019 - 2021
// MIT License

#ifndef BOOTNOTIFICATION_H
#define BOOTNOTIFICATION_H

#include <ArduinoOcpp/Core/OcppMessage.h>


namespace ArduinoOcpp {
namespace Ocpp16 {

class BootNotification : public OcppMessage {
private:
  String chargePointModel = String('\0');
  String chargePointVendor = String('\0');
  String chargePointSerialNumber = String('\0');
  String firmwareVersion = String('\0');
public:
  BootNotification();

  BootNotification(String &chargePointModel, String &chargePointVendor);

  BootNotification(String &chargePointModel, String &chargePointVendor, String &chargePointSerialNumber);

  BootNotification(String &chargePointModel, String &chargePointVendor, String &chargePointSerialNumber, String &firmwareVersion);

  const char* getOcppOperationType();

  DynamicJsonDocument* createReq();

  void processConf(JsonObject payload);

  void processReq(JsonObject payload);

  DynamicJsonDocument* createConf();
};

} //end namespace Ocpp16
} //end namespace ArduinoOcpp

#endif
