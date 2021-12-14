// matth-x/ArduinoOcpp
// Copyright Matthias Akstaller 2019 - 2021
// MIT License

#ifndef REMOTESTARTTRANSACTION_H
#define REMOTESTARTTRANSACTION_H

#include <ArduinoOcpp/Core/OcppMessage.h>

#include <Variants.h>

namespace ArduinoOcpp {
namespace Ocpp16 {

class RemoteStartTransaction : public OcppMessage {
private:
    int connectorId;
public:
    RemoteStartTransaction();

    const char* getOcppOperationType();

    std::unique_ptr<DynamicJsonDocument> createReq();

    void processConf(JsonObject payload);

    void processReq(JsonObject payload);

    std::unique_ptr<DynamicJsonDocument> createConf();
};

} //end namespace Ocpp16
} //end namespace ArduinoOcpp
#endif
