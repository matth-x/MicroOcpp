// matth-x/MicroOcpp
// Copyright Matthias Akstaller 2019 - 2023
// MIT License

#ifndef SETCHARGINGPROFILE_H
#define SETCHARGINGPROFILE_H

#include <MicroOcpp/Core/Operation.h>

namespace MicroOcpp {

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

    const char* getOperationType() override;

    void processReq(JsonObject payload) override;

    std::unique_ptr<DynamicJsonDocument> createConf() override;

    const char *getErrorCode() override {return errorCode;}
    const char *getErrorDescription() override {return errorDescription;}
};

} //end namespace Ocpp16
} //end namespace MicroOcpp
#endif
