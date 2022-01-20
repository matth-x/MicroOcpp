// matth-x/ArduinoOcpp
// Copyright Matthias Akstaller 2019 - 2022
// MIT License

#ifndef TRIGGERMESSAGE_H
#define TRIGGERMESSAGE_H

#include <ArduinoOcpp/Core/OcppMessage.h>
#include <Variants.h>

#include <memory>

namespace ArduinoOcpp {

class OcppOperation;

namespace Ocpp16 {

class TriggerMessage : public OcppMessage {
private:
    std::unique_ptr<OcppOperation> triggeredOperation;
    const char *statusMessage;
public:
    TriggerMessage();

    const char* getOcppOperationType();

    void processReq(JsonObject payload);

    std::unique_ptr<DynamicJsonDocument> createConf();
};

} //end namespace Ocpp16
} //end namespace ArduinoOcpp
#endif
