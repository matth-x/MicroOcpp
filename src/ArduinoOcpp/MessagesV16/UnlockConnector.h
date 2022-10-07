// matth-x/ArduinoOcpp
// Copyright Matthias Akstaller 2019 - 2022
// MIT License

#ifndef UNLOCKCONNECTOR_H
#define UNLOCKCONNECTOR_H

#include <ArduinoOcpp/Core/OcppMessage.h>
#include <ArduinoOcpp/Core/PollResult.h>
#include <functional>

namespace ArduinoOcpp {
namespace Ocpp16 {

class UnlockConnector : public OcppMessage {
private:
    bool err = false;
    std::function<PollResult<bool> ()> unlockConnector;
    PollResult<bool> cbUnlockResult;
public:
    UnlockConnector();

    const char* getOcppOperationType();

    void processReq(JsonObject payload);

    std::unique_ptr<DynamicJsonDocument> createConf();
};

} //end namespace Ocpp16
} //end namespace ArduinoOcpp
#endif
