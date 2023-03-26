// matth-x/ArduinoOcpp
// Copyright Matthias Akstaller 2019 - 2023
// MIT License

#ifndef REMOTESTOPTRANSACTION_H
#define REMOTESTOPTRANSACTION_H

#include <ArduinoOcpp/Core/OcppMessage.h>

namespace ArduinoOcpp {

class OcppModel;

namespace Ocpp16 {

class RemoteStopTransaction : public OcppMessage {
private:
    OcppModel& context;
    int transactionId;
public:
    RemoteStopTransaction(OcppModel& context);

    const char* getOcppOperationType();

    void processReq(JsonObject payload);

    std::unique_ptr<DynamicJsonDocument> createConf();
};

} //end namespace Ocpp16
} //end namespace ArduinoOcpp
#endif
