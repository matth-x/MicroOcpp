// matth-x/MicroOcpp
// Copyright Matthias Akstaller 2019 - 2024
// MIT License

#ifndef UNLOCKCONNECTOR_H
#define UNLOCKCONNECTOR_H

#include <MicroOcpp/Core/Operation.h>
#include <MicroOcpp/Model/ConnectorBase/Connector.h>
#include <MicroOcpp/Model/ConnectorBase/UnlockConnectorResult.h>
#include <functional>

namespace MicroOcpp {

class Model;

namespace Ocpp16 {

class UnlockConnector : public Operation {
private:
    Model& model;
    std::function<UnlockConnectorResult ()> unlockConnector;
    Connector *connector = nullptr;
    UnlockConnectorResult cbUnlockResult;
    unsigned long timerStart = 0; //for timeout

    const char *errorCode = nullptr;
public:
    UnlockConnector(Model& model);

    const char* getOperationType() override;

    void processReq(JsonObject payload) override;

    std::unique_ptr<DynamicJsonDocument> createConf() override;

    const char *getErrorCode() override {return errorCode;}
};

} //end namespace Ocpp16
} //end namespace MicroOcpp
#endif
