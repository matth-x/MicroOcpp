// matth-x/ArduinoOcpp
// Copyright Matthias Akstaller 2019 - 2022
// MIT License

#ifndef GETCONFIGURATION_H
#define GETCONFIGURATION_H

#include <ArduinoOcpp/Core/OcppMessage.h>

namespace ArduinoOcpp {
namespace Ocpp16 {

class GetConfiguration : public OcppMessage {
private:
    std::vector<std::string> keys;
public:
    GetConfiguration();

    const char* getOcppOperationType();

    void processReq(JsonObject payload);

    std::unique_ptr<DynamicJsonDocument> createConf();

};

} //end namespace Ocpp16
} //end namespace ArduinoOcpp
#endif
