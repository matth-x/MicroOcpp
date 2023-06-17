// matth-x/ArduinoOcpp
// Copyright Matthias Akstaller 2019 - 2023
// MIT License

#ifndef TRIGGERMESSAGE_H
#define TRIGGERMESSAGE_H

#include <ArduinoOcpp/Core/Operation.h>

#include <vector>

namespace ArduinoOcpp {

class Context;
class Request;

namespace Ocpp16 {

class TriggerMessage : public Operation {
private:
    Context& context;
    std::vector<std::unique_ptr<Request>> triggeredOperations;
    const char *statusMessage {nullptr};

    const char *errorCode = nullptr;
public:
    TriggerMessage(Context& context);

    const char* getOperationType();

    void processReq(JsonObject payload);

    std::unique_ptr<DynamicJsonDocument> createConf();

    const char *getErrorCode() {return errorCode;}
};

} //end namespace Ocpp16
} //end namespace ArduinoOcpp
#endif
