// matth-x/ArduinoOcpp
// Copyright Matthias Akstaller 2019 - 2023
// MIT License

#ifndef SETCHARGINGPROFILE_H
#define SETCHARGINGPROFILE_H

#include <ArduinoOcpp/Core/Operation.h>

namespace ArduinoOcpp {

class Model;
class SmartChargingService;

namespace Ocpp16 {

class SetChargingProfile : public Operation {
private:
    Model& model;
    SmartChargingService& scService;

    bool accepted = false;
    const char *errorCode = nullptr;
    const char *errorDescription = "";
public:
    SetChargingProfile(Model& model, SmartChargingService& scService);

    ~SetChargingProfile();

    const char* getOperationType();

    void processReq(JsonObject payload);

    std::unique_ptr<DynamicJsonDocument> createConf();

    const char *getErrorCode() override {return errorCode;}
    const char *getErrorDescription() override {return errorDescription;}
};

} //end namespace Ocpp16
} //end namespace ArduinoOcpp
#endif
