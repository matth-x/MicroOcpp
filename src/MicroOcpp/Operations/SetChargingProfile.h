// matth-x/MicroOcpp
// Copyright Matthias Akstaller 2019 - 2024
// MIT License

#ifndef MO_SETCHARGINGPROFILE_H
#define MO_SETCHARGINGPROFILE_H

#include <MicroOcpp/Core/Operation.h>
#include <MicroOcpp/Version.h>

#if (MO_ENABLE_V16 || MO_ENABLE_V201) && MO_ENABLE_SMARTCHARGING

namespace MicroOcpp {

class Context;
class SmartChargingService;

class SetChargingProfile : public Operation, public MemoryManaged {
private:
    Context& context;
    SmartChargingService& scService;
    int ocppVersion = -1;

    bool accepted = false;
    const char *errorCode = nullptr;
    const char *errorDescription = "";
public:
    SetChargingProfile(Context& context, SmartChargingService& scService);

    ~SetChargingProfile();

    const char* getOperationType() override;

    void processReq(JsonObject payload) override;

    std::unique_ptr<JsonDoc> createConf() override;

    const char *getErrorCode() override {return errorCode;}
    const char *getErrorDescription() override {return errorDescription;}
};

} //namespace MicroOcpp
#endif //(MO_ENABLE_V16 || MO_ENABLE_V201) && MO_ENABLE_SMARTCHARGING
#endif
