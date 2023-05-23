// matth-x/ArduinoOcpp
// Copyright Matthias Akstaller 2019 - 2023
// MIT License

#ifndef AUTHORIZE_H
#define AUTHORIZE_H

#include <ArduinoOcpp/Core/Operation.h>
#include <ArduinoOcpp/MessagesV16/CiStrings.h>

namespace ArduinoOcpp {

class Model;

namespace Ocpp16 {

class Authorize : public Operation {
private:
    Model& model;
    char idTag [IDTAG_LEN_MAX + 1] = {'\0'};
public:
//    Authorize();

    Authorize(Model& model, const char *idTag);

    const char* getOperationType();

    std::unique_ptr<DynamicJsonDocument> createReq();

    void processConf(JsonObject payload);

    void processReq(JsonObject payload);

    std::unique_ptr<DynamicJsonDocument> createConf();

};

} //end namespace Ocpp16
} //end namespace ArduinoOcpp

#endif
