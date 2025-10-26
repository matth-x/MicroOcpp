// matth-x/MicroOcpp
// Copyright Matthias Akstaller 2019 - 2024
// MIT License

#ifndef MO_GETCOMPOSITESCHEDULE_H
#define MO_GETCOMPOSITESCHEDULE_H

#include <MicroOcpp/Core/Operation.h>
#include <MicroOcpp/Model/SmartCharging/SmartChargingModel.h>
#include <MicroOcpp/Version.h>

#if (MO_ENABLE_V16 || MO_ENABLE_V201) && MO_ENABLE_SMARTCHARGING

namespace MicroOcpp {

class Context;
class SmartChargingService;

class GetCompositeSchedule : public Operation, public MemoryManaged {
private:
    Context& context;
    SmartChargingService& scService;
    int ocppVersion = -1;
    int evseId = -1;
    int duration = -1;
    ChargingRateUnitType chargingRateUnit = ChargingRateUnitType::UNDEFINED;

    const char *errorCode {nullptr};
public:
    GetCompositeSchedule(Context& context, SmartChargingService& scService);

    const char* getOperationType() override;

    void processReq(JsonObject payload) override;

    std::unique_ptr<JsonDoc> createConf() override;

    const char *getErrorCode() override {return errorCode;}
};

} //namespace MicroOcpp
#endif //(MO_ENABLE_V16 || MO_ENABLE_V201) && MO_ENABLE_SMARTCHARGING
#endif
