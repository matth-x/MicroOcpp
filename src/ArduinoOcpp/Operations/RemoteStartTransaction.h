// matth-x/ArduinoOcpp
// Copyright Matthias Akstaller 2019 - 2023
// MIT License

#ifndef REMOTESTARTTRANSACTION_H
#define REMOTESTARTTRANSACTION_H

#include <ArduinoOcpp/Core/Operation.h>
#include <ArduinoOcpp/Operations/CiStrings.h>

namespace ArduinoOcpp {

class Model;

namespace Ocpp16 {

class RemoteStartTransaction : public Operation {
private:
    Model& model;
    int connectorId;
    char idTag [IDTAG_LEN_MAX + 1] = {'\0'};
    DynamicJsonDocument chargingProfileDoc {0};
    
    const char *errorCode {nullptr};
public:
    RemoteStartTransaction(Model& model);

    const char* getOperationType();

    std::unique_ptr<DynamicJsonDocument> createReq();

    void processConf(JsonObject payload);

    void processReq(JsonObject payload);

    std::unique_ptr<DynamicJsonDocument> createConf();

    const char *getErrorCode() {return errorCode;}
};

} //end namespace Ocpp16
} //end namespace ArduinoOcpp
#endif
