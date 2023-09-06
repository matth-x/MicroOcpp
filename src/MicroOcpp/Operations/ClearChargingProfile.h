// matth-x/MicroOcpp
// Copyright Matthias Akstaller 2019 - 2023
// MIT License

#ifndef CLEARCHARGINGPROFILE_H
#define CLEARCHARGINGPROFILE_H

#include <MicroOcpp/Core/Operation.h>

namespace MicroOcpp {

class SmartChargingService;

namespace Ocpp16 {

class ClearChargingProfile : public Operation {
private:
    SmartChargingService& scService;
    bool matchingProfilesFound = false;
public:
    ClearChargingProfile(SmartChargingService& scService);

    const char* getOperationType() override;

    void processReq(JsonObject payload) override;

    std::unique_ptr<DynamicJsonDocument> createConf() override;
    
};

} //end namespace Ocpp16
} //end namespace MicroOcpp
#endif
