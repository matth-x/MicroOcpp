// matth-x/ArduinoOcpp
// Copyright Matthias Akstaller 2019 - 2022
// MIT License

#ifndef AUTHORIZE_H
#define AUTHORIZE_H

#include <ArduinoOcpp/Core/OcppMessage.h>

namespace ArduinoOcpp {
namespace Ocpp16 {

class Authorize : public OcppMessage {
private:
    String idTag;
public:
    Authorize();

    Authorize(const String &idTag);

    const char* getOcppOperationType();

    std::unique_ptr<DynamicJsonDocument> createReq();

    void processConf(JsonObject payload);

    void processReq(JsonObject payload);

    std::unique_ptr<DynamicJsonDocument> createConf();

};

} //end namespace Ocpp16
} //end namespace ArduinoOcpp

#endif
