// matth-x/ArduinoOcpp
// Copyright Matthias Akstaller 2019 - 2021
// MIT License

#ifndef SETCHARGINGPROFILE_H
#define SETCHARGINGPROFILE_H

#include <ArduinoOcpp/Core/OcppMessage.h>

namespace ArduinoOcpp {
namespace Ocpp16 {

class SetChargingProfile : public OcppMessage {
private:
    std::unique_ptr<DynamicJsonDocument> payloadToClient;
public:
    SetChargingProfile();

    SetChargingProfile(std::unique_ptr<DynamicJsonDocument> payloadToClient);

    ~SetChargingProfile();

    const char* getOcppOperationType();

    void processReq(JsonObject payload);

    std::unique_ptr<DynamicJsonDocument> createConf();

    std::unique_ptr<DynamicJsonDocument> createReq();

    void processConf(JsonObject payload);
};

} //end namespace Ocpp16
} //end namespace ArduinoOcpp
#endif
