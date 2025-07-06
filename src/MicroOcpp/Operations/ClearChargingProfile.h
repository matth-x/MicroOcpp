// matth-x/MicroOcpp
// Copyright Matthias Akstaller 2019 - 2024
// MIT License

#ifndef MO_CLEARCHARGINGPROFILE_H
#define MO_CLEARCHARGINGPROFILE_H

#include <MicroOcpp/Core/Operation.h>
#include <MicroOcpp/Version.h>

#if (MO_ENABLE_V16 || MO_ENABLE_V201) && MO_ENABLE_SMARTCHARGING

namespace MicroOcpp {

class SmartChargingService;

class ClearChargingProfile : public Operation, public MemoryManaged {
private:
    SmartChargingService& scService;
    int ocppVersion = -1;

    const char *status = "";
    const char *errorCode = nullptr;
public:
    ClearChargingProfile(SmartChargingService& scService, int ocppVersion);

    const char* getOperationType() override;

    void processReq(JsonObject payload) override;

    std::unique_ptr<JsonDoc> createConf() override;

    const char *getErrorCode() override {return errorCode;}
};

} //namespace MicroOcpp
#endif //(MO_ENABLE_V16 || MO_ENABLE_V201) && MO_ENABLE_SMARTCHARGING
#endif
