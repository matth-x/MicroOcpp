// matth-x/ArduinoOcpp
// Copyright Matthias Akstaller 2019 - 2023
// MIT License

#ifndef REMOTESTARTTRANSACTION_H
#define REMOTESTARTTRANSACTION_H

#include <ArduinoOcpp/Core/Operation.h>
#include <ArduinoOcpp/Operations/CiStrings.h>

namespace ArduinoOcpp {

class Model;
class ChargingProfile;

namespace Ocpp16 {

class RemoteStartTransaction : public Operation {
private:
    Model& model;
    int connectorId;
    char idTag [IDTAG_LEN_MAX + 1] = {'\0'};

    std::unique_ptr<ChargingProfile> chargingProfile;
    
    const char *errorCode {nullptr};
    const char *errorDescription = "";
public:
    RemoteStartTransaction(Model& model);

    const char* getOperationType();

    std::unique_ptr<DynamicJsonDocument> createReq();

    void processConf(JsonObject payload);

    void processReq(JsonObject payload);

    std::unique_ptr<DynamicJsonDocument> createConf();

    const char *getErrorCode() {return errorCode;}
    const char *getErrorDescription() override {return errorDescription;}
};

} //end namespace Ocpp16
} //end namespace ArduinoOcpp
#endif
