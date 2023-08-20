// matth-x/ArduinoOcpp
// Copyright Matthias Akstaller 2019 - 2023
// MIT License

#ifndef CLEARCHARGINGPROFILE_H
#define CLEARCHARGINGPROFILE_H

#include <ArduinoOcpp/Core/Operation.h>

namespace ArduinoOcpp {

class SmartChargingService;

namespace Ocpp16 {

class ClearChargingProfile : public Operation {
private:
    SmartChargingService& scService;
    bool matchingProfilesFound = false;
public:
    ClearChargingProfile(SmartChargingService& scService);

    const char* getOperationType();

    void processReq(JsonObject payload);

    std::unique_ptr<DynamicJsonDocument> createConf();
    
};

} //end namespace Ocpp16
} //end namespace ArduinoOcpp
#endif
