// matth-x/ArduinoOcpp
// Copyright Matthias Akstaller 2019 - 2023
// MIT License

#ifndef SETCHARGINGPROFILE_H
#define SETCHARGINGPROFILE_H

#include <ArduinoOcpp/Core/Operation.h>

namespace ArduinoOcpp {

class Model;

namespace Ocpp16 {

class SetChargingProfile : public Operation {
private:
    Model& model;
    std::unique_ptr<DynamicJsonDocument> payloadToClient;
public:
    SetChargingProfile(Model& model);

    SetChargingProfile(Model& model, std::unique_ptr<DynamicJsonDocument> payloadToClient);

    ~SetChargingProfile();

    const char* getOperationType();

    void processReq(JsonObject payload);

    std::unique_ptr<DynamicJsonDocument> createConf();

    std::unique_ptr<DynamicJsonDocument> createReq();

    void processConf(JsonObject payload);
};

} //end namespace Ocpp16
} //end namespace ArduinoOcpp
#endif
