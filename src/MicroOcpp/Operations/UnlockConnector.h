// matth-x/MicroOcpp
// Copyright Matthias Akstaller 2019 - 2023
// MIT License

#ifndef UNLOCKCONNECTOR_H
#define UNLOCKCONNECTOR_H

#include <MicroOcpp/Core/Operation.h>
#include <MicroOcpp/Core/PollResult.h>
#include <functional>

namespace MicroOcpp {

class Model;

namespace Ocpp16 {

class UnlockConnector : public Operation {
private:
    Model& model;
    bool err = false;
    std::function<PollResult<bool> ()> unlockConnector;
    PollResult<bool> cbUnlockResult;
    unsigned long timerStart = 0; //for timeout
public:
    UnlockConnector(Model& model);

    const char* getOperationType() override;

    void processReq(JsonObject payload) override;

    std::unique_ptr<DynamicJsonDocument> createConf() override;
};

} //end namespace Ocpp16
} //end namespace MicroOcpp
#endif
