// matth-x/MicroOcpp
// Copyright Matthias Akstaller 2019 - 2024
// MIT License

#ifndef UNLOCKCONNECTOR_H
#define UNLOCKCONNECTOR_H

#include <functional>

#include <MicroOcpp/Core/Operation.h>
#include <MicroOcpp/Core/Time.h>
#include <MicroOcpp/Model/RemoteControl/RemoteControlDefs.h>
#include <MicroOcpp/Version.h>

#if MO_ENABLE_V16

namespace MicroOcpp {

class Context;
class RemoteControlService;
class RemoteControlServiceEvse;

namespace Ocpp16 {

class UnlockConnector : public Operation, public MemoryManaged {
private:
    Context& context;
    RemoteControlService& rcService;
    RemoteControlServiceEvse *rcEvse = nullptr;

    UnlockStatus status;
    Timestamp timerStart; //for timeout

    const char *errorCode = nullptr;
public:
    UnlockConnector(Context& context, RemoteControlService& rcService);

    const char* getOperationType() override;

    void processReq(JsonObject payload) override;

    std::unique_ptr<JsonDoc> createConf() override;

    const char *getErrorCode() override {return errorCode;}
};

} //namespace Ocpp16
} //namespace MicroOcpp
#endif //MO_ENABLE_V16

#if MO_ENABLE_V201 && MO_ENABLE_CONNECTOR_LOCK

#include <MicroOcpp/Model/RemoteControl/RemoteControlDefs.h>

namespace MicroOcpp {

class Context;
class RemoteControlService;
class RemoteControlServiceEvse;

namespace Ocpp201 {

class UnlockConnector : public Operation, public MemoryManaged {
private:
    Context& context;
    RemoteControlService& rcService;
    RemoteControlServiceEvse *rcEvse = nullptr;

    UnlockStatus status;
    Timestamp timerStart; //for timeout

    const char *errorCode = nullptr;
public:
    UnlockConnector(Context& context, RemoteControlService& rcService);

    const char* getOperationType() override;

    void processReq(JsonObject payload) override;

    std::unique_ptr<JsonDoc> createConf() override;

    const char *getErrorCode() override {return errorCode;}
};

} //namespace Ocpp201
} //namespace MicroOcpp
#endif //MO_ENABLE_V201 && MO_ENABLE_CONNECTOR_LOCK
#endif
