// matth-x/ArduinoOcpp
// Copyright Matthias Akstaller 2019 - 2023
// MIT License

#ifndef GETCOMPOSITESCHEDULE_H
#define GETCOMPOSITESCHEDULE_H

#include <ArduinoOcpp/Core/Operation.h>
#include <ArduinoOcpp/Core/Time.h>
#include <ArduinoOcpp/Model/SmartCharging/SmartChargingModel.h>

namespace ArduinoOcpp {

class Model;

namespace Ocpp16 {

class GetCompositeSchedule : public Operation {
private:
    Model& model;
    int connectorId {-1};
    int duration {0};
    ChargingRateUnitType chargingRateUnit {ChargingRateUnitType::Watt};

    const char *errorCode {nullptr};
public:
    GetCompositeSchedule(Model& model);

    const char* getOperationType();

    void processReq(JsonObject payload);

    std::unique_ptr<DynamicJsonDocument> createConf();

    const char *getErrorCode() {return errorCode;}
};

} //end namespace Ocpp16
} //end namespace ArduinoOcpp
#endif
