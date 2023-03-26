// matth-x/ArduinoOcpp
// Copyright Matthias Akstaller 2019 - 2023
// MIT License

#ifndef RESERVENOW_H
#define RESERVENOW_H

#include <ArduinoOcpp/Core/OcppMessage.h>

namespace ArduinoOcpp {

class OcppModel;

namespace Ocpp16 {

class ReserveNow : public OcppMessage {
private:
    OcppModel& context;
    const char *errorCode = nullptr;
    const char *reservationStatus = nullptr;
public:
    ReserveNow(OcppModel& context);

    ~ReserveNow();

    const char* getOcppOperationType();

    void processReq(JsonObject payload);

    std::unique_ptr<DynamicJsonDocument> createConf();

    const char *getErrorCode() {return errorCode;}
};

} //end namespace Ocpp16
} //end namespace ArduinoOcpp

#endif
