// matth-x/ArduinoOcpp
// Copyright Matthias Akstaller 2019 - 2023
// MIT License

#ifndef SENDLOCALLIST_H
#define SENDLOCALLIST_H

#include <ArduinoOcpp/Core/OcppMessage.h>

namespace ArduinoOcpp {
namespace Ocpp16 {

class SendLocalList : public OcppMessage {
private:
    const char *errorCode = nullptr;
    bool updateFailure = true;
    bool versionMismatch = false;
public:
    SendLocalList();

    ~SendLocalList();

    const char* getOcppOperationType();

    void processReq(JsonObject payload);

    std::unique_ptr<DynamicJsonDocument> createConf();

    const char *getErrorCode() {return errorCode;}
};

} //end namespace Ocpp16
} //end namespace ArduinoOcpp
#endif
