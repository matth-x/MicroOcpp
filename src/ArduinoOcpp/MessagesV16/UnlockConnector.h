// matth-x/ArduinoOcpp
// Copyright Matthias Akstaller 2019 - 2023
// MIT License

#ifndef UNLOCKCONNECTOR_H
#define UNLOCKCONNECTOR_H

#include <ArduinoOcpp/Core/OcppMessage.h>
#include <ArduinoOcpp/Core/PollResult.h>
#include <functional>

namespace ArduinoOcpp {

class OcppModel;

namespace Ocpp16 {

class UnlockConnector : public OcppMessage {
private:
    OcppModel& context;
    bool err = false;
    std::function<PollResult<bool> ()> unlockConnector;
    PollResult<bool> cbUnlockResult;
    unsigned long timerStart = 0; //for timeout
public:
    UnlockConnector(OcppModel& context);

    const char* getOcppOperationType();

    void processReq(JsonObject payload);

    std::unique_ptr<DynamicJsonDocument> createConf();
};

} //end namespace Ocpp16
} //end namespace ArduinoOcpp
#endif
