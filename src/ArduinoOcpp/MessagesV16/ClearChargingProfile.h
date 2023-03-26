// matth-x/ArduinoOcpp
// Copyright Matthias Akstaller 2019 - 2023
// MIT License

#ifndef CLEARCHARGINGPROFILE_H
#define CLEARCHARGINGPROFILE_H

#include <ArduinoOcpp/Core/OcppMessage.h>

namespace ArduinoOcpp {

class OcppModel;

namespace Ocpp16 {

class ClearChargingProfile : public OcppMessage {
private:
    OcppModel& context;
    bool matchingProfilesFound = false;
    const char *errorCode;
public:
    ClearChargingProfile(OcppModel& context);

    const char* getOcppOperationType();

    void processReq(JsonObject payload);

    std::unique_ptr<DynamicJsonDocument> createConf();

    const char *getErrorCode() {return errorCode;}
    
};

} //end namespace Ocpp16
} //end namespace ArduinoOcpp
#endif
