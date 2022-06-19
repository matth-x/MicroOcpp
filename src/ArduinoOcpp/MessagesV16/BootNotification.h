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
    std::unique_ptr<DynamicJsonDocument> credentials;
public:
    BootNotification();

    ~BootNotification() = default;

    BootNotification(std::unique_ptr<DynamicJsonDocument> payload);

    const char* getOcppOperationType();

    void initiate();

    std::unique_ptr<DynamicJsonDocument> createReq();

    void processConf(JsonObject payload);

    void processReq(JsonObject payload);

    std::unique_ptr<DynamicJsonDocument> createConf();
};

} //end namespace Ocpp16
} //end namespace ArduinoOcpp

#endif
