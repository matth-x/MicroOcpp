// matth-x/ArduinoOcpp
// Copyright Matthias Akstaller 2019 - 2023
// MIT License

#ifndef CLEARCHARGINGPROFILE_H
#define CLEARCHARGINGPROFILE_H

#include <ArduinoOcpp/Core/Operation.h>

namespace ArduinoOcpp {

class Model;

namespace Ocpp16 {

class ClearChargingProfile : public Operation {
private:
    Model& model;
    bool matchingProfilesFound = false;
    const char *errorCode;
public:
    ClearChargingProfile(Model& model);

    const char* getOperationType();

    void processReq(JsonObject payload);

    std::unique_ptr<DynamicJsonDocument> createConf();

    const char *getErrorCode() {return errorCode;}
    
};

} //end namespace Ocpp16
} //end namespace ArduinoOcpp
#endif
