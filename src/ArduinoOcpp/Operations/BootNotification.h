// matth-x/ArduinoOcpp
// Copyright Matthias Akstaller 2019 - 2023
// MIT License

#ifndef BOOTNOTIFICATION_H
#define BOOTNOTIFICATION_H

#include <ArduinoOcpp/Core/Operation.h>
#include <ArduinoOcpp/Operations/CiStrings.h>

#define CP_MODEL_LEN_MAX        CiString20TypeLen
#define CP_SERIALNUMBER_LEN_MAX CiString25TypeLen
#define CP_VENDOR_LEN_MAX       CiString20TypeLen
#define FW_VERSION_LEN_MAX      CiString50TypeLen

namespace ArduinoOcpp {

class Model;

namespace Ocpp16 {

class BootNotification : public Operation {
private:
    Model& model;
    std::unique_ptr<DynamicJsonDocument> credentials;
    const char *errorCode = nullptr;
public:
    BootNotification(Model& model, std::unique_ptr<DynamicJsonDocument> payload);

    ~BootNotification() = default;

    const char* getOperationType();

    std::unique_ptr<DynamicJsonDocument> createReq();

    void processConf(JsonObject payload);

    void processReq(JsonObject payload);

    std::unique_ptr<DynamicJsonDocument> createConf();

    const char *getErrorCode() {return errorCode;}
};

} //end namespace Ocpp16
} //end namespace ArduinoOcpp

#endif
