// matth-x/ArduinoOcpp
// Copyright Matthias Akstaller 2019 - 2023
// MIT License

#ifndef GETCOMPOSITESCHEDULE_H
#define GETCOMPOSITESCHEDULE_H

#include <ArduinoOcpp/Core/OcppMessage.h>
#include <ArduinoOcpp/Core/OcppTime.h>
#include <ArduinoOcpp/Tasks/SmartCharging/SmartChargingModel.h>

namespace ArduinoOcpp {

class OcppModel;

namespace Ocpp16 {

class GetCompositeSchedule : public OcppMessage {
private:
    OcppModel& context;
    int connectorId {-1};
    otime_t duration {0};
    ChargingRateUnitType chargingRateUnit {ChargingRateUnitType::Watt};

    const char *errorCode {nullptr};
public:
    GetCompositeSchedule(OcppModel& context);

    const char* getOcppOperationType();

    void processReq(JsonObject payload);

    std::unique_ptr<DynamicJsonDocument> createConf();

    const char *getErrorCode() {return errorCode;}
};

} //end namespace Ocpp16
} //end namespace ArduinoOcpp
#endif
