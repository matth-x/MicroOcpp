// matth-x/MicroOcpp
// Copyright Matthias Akstaller 2019 - 2024
// MIT License

#ifndef UNLOCKCONNECTOR_H
#define UNLOCKCONNECTOR_H

#include <MicroOcpp/Version.h>

#include <MicroOcpp/Core/Operation.h>
#include <MicroOcpp/Model/ConnectorBase/UnlockConnectorResult.h>
#include <functional>

namespace MicroOcpp {

class Model;

namespace Ocpp16 {

class UnlockConnector : public Operation, public MemoryManaged {
private:
    Model& model;

#if MO_ENABLE_CONNECTOR_LOCK
    std::function<UnlockConnectorResult ()> unlockConnector;
    UnlockConnectorResult cbUnlockResult;
    unsigned long timerStart = 0; //for timeout
#endif //MO_ENABLE_CONNECTOR_LOCK

    const char *errorCode = nullptr;
public:
    UnlockConnector(Model& model);

    const char* getOperationType() override;

    void processReq(JsonObject payload) override;

    std::unique_ptr<JsonDoc> createConf() override;

    const char *getErrorCode() override {return errorCode;}
};

} //end namespace Ocpp16
} //end namespace MicroOcpp

#if MO_ENABLE_V201
#if MO_ENABLE_CONNECTOR_LOCK

#include <MicroOcpp/Model/RemoteControl/RemoteControlDefs.h>

namespace MicroOcpp {

class RemoteControlService;
class RemoteControlServiceEvse;

namespace Ocpp201 {

class UnlockConnector : public Operation, public MemoryManaged {
private:
    RemoteControlService& rcService;
    RemoteControlServiceEvse *rcEvse = nullptr;

    UnlockStatus status;
    unsigned long timerStart = 0; //for timeout

    const char *errorCode = nullptr;
public:
    UnlockConnector(RemoteControlService& rcService);

    const char* getOperationType() override;

    void processReq(JsonObject payload) override;

    std::unique_ptr<JsonDoc> createConf() override;

    const char *getErrorCode() override {return errorCode;}
};

} //end namespace Ocpp201
} //end namespace MicroOcpp

#endif //MO_ENABLE_CONNECTOR_LOCK
#endif //MO_ENABLE_V201

#endif
