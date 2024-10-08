// matth-x/MicroOcpp
// Copyright Matthias Akstaller 2019 - 2024
// MIT License

#ifndef MO_GETCOMPOSITESCHEDULE_H
#define MO_GETCOMPOSITESCHEDULE_H

#include <MicroOcpp/Core/Operation.h>
#include <MicroOcpp/Core/Time.h>
#include <MicroOcpp/Model/SmartCharging/SmartChargingService.h>

namespace MicroOcpp {

class Model;

namespace Ocpp16 {

class GetCompositeSchedule : public Operation, public MemoryManaged {
private:
    Model& model;
    SmartChargingService& scService;
    int connectorId = -1;
    int duration = -1;
    ChargingRateUnitType_Optional chargingRateUnit = ChargingRateUnitType_Optional::None;

    const char *errorCode {nullptr};
public:
    GetCompositeSchedule(Model& model, SmartChargingService& scService);

    const char* getOperationType() override;

    void processReq(JsonObject payload) override;

    std::unique_ptr<JsonDoc> createConf() override;

    const char *getErrorCode() override {return errorCode;}
};

} //end namespace Ocpp16
} //end namespace MicroOcpp
#endif
