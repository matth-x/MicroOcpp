// matth-x/ArduinoOcpp
// Copyright Matthias Akstaller 2019 - 2022
// MIT License

#ifndef AUTHORIZE_H
#define AUTHORIZE_H

#include <ArduinoOcpp/Core/OcppMessage.h>
#include <ArduinoOcpp/MessagesV16/CiStrings.h>

namespace ArduinoOcpp {
namespace Ocpp16 {

class Authorize : public OcppMessage {
private:
    char idTag [IDTAG_LEN_MAX + 1] = {'\0'};
public:
//    Authorize();

    Authorize(const char *idTag);

    const char* getOcppOperationType();

    std::unique_ptr<DynamicJsonDocument> createReq();

    void processConf(JsonObject payload);

    void processReq(JsonObject payload);

    std::unique_ptr<DynamicJsonDocument> createConf();

};

} //end namespace Ocpp16
} //end namespace ArduinoOcpp

#endif
