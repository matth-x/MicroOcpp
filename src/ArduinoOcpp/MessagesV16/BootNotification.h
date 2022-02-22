// matth-x/ArduinoOcpp
// Copyright Matthias Akstaller 2019 - 2022
// MIT License

#ifndef BOOTNOTIFICATION_H
#define BOOTNOTIFICATION_H

#include <ArduinoOcpp/Core/OcppMessage.h>
#include <ArduinoOcpp/MessagesV16/CiStrings.h>

#define CP_MODEL_LEN_MAX        CiString20TypeLen
#define CP_SERIALNUMBER_LEN_MAX CiString25TypeLen
#define CP_VENDOR_LEN_MAX       CiString20TypeLen
#define FW_VERSION_LEN_MAX      CiString50TypeLen

namespace ArduinoOcpp {
namespace Ocpp16 {

class BootNotification : public OcppMessage {
private:
    char chargePointModel [CP_MODEL_LEN_MAX + 1] = {'\0'};
    char chargePointSerialNumber [CP_SERIALNUMBER_LEN_MAX + 1] = {'\0'};
    char chargePointVendor [CP_VENDOR_LEN_MAX + 1] = {'\0'};
    char firmwareVersion [FW_VERSION_LEN_MAX + 1] = {'\0'};

    DynamicJsonDocument *overridePayload = NULL;
public:
    BootNotification();

    ~BootNotification();

    BootNotification(const char *chargePointModel, const char *chargePointVendor);

    BootNotification(const char *chargePointModel, const char *chargePointSerialNumber, const char *chargePointVendor, const char *firmwareVersion);

    BootNotification(DynamicJsonDocument *payload);

    const char* getOcppOperationType();

    std::unique_ptr<DynamicJsonDocument> createReq();

    void processConf(JsonObject payload);

    void processReq(JsonObject payload);

    std::unique_ptr<DynamicJsonDocument> createConf();
};

} //end namespace Ocpp16
} //end namespace ArduinoOcpp

#endif
