// matth-x/ArduinoOcpp
// Copyright Matthias Akstaller 2019 - 2023
// MIT License

#ifndef REMOTESTARTTRANSACTION_H
#define REMOTESTARTTRANSACTION_H

#include <ArduinoOcpp/Core/OcppMessage.h>
#include <ArduinoOcpp/MessagesV16/CiStrings.h>

namespace ArduinoOcpp {

class OcppModel;

namespace Ocpp16 {

class RemoteStartTransaction : public OcppMessage {
private:
    OcppModel& context;
    int connectorId;
    char idTag [IDTAG_LEN_MAX + 1] = {'\0'};
    DynamicJsonDocument chargingProfileDoc {0};
    
    const char *errorCode {nullptr};
public:
    RemoteStartTransaction(OcppModel& context);

    const char* getOcppOperationType();

    std::unique_ptr<DynamicJsonDocument> createReq();

    void processConf(JsonObject payload);

    void processReq(JsonObject payload);

    std::unique_ptr<DynamicJsonDocument> createConf();

    const char *getErrorCode() {return errorCode;}
};

} //end namespace Ocpp16
} //end namespace ArduinoOcpp
#endif
