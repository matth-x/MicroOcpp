// matth-x/ArduinoOcpp
// Copyright Matthias Akstaller 2019 - 2023
// MIT License

#ifndef TRIGGERMESSAGE_H
#define TRIGGERMESSAGE_H

#include <ArduinoOcpp/Core/OcppMessage.h>

#include <vector>

namespace ArduinoOcpp {

class OcppModel;
class OcppOperation;

namespace Ocpp16 {

class TriggerMessage : public OcppMessage {
private:
    OcppModel& context;
    std::vector<std::unique_ptr<OcppOperation>> triggeredOperations;
    const char *statusMessage {nullptr};

    const char *errorCode = nullptr;
public:
    TriggerMessage(OcppModel& context);

    const char* getOcppOperationType();

    void processReq(JsonObject payload);

    std::unique_ptr<DynamicJsonDocument> createConf();

    const char *getErrorCode() {return errorCode;}
};

} //end namespace Ocpp16
} //end namespace ArduinoOcpp
#endif
