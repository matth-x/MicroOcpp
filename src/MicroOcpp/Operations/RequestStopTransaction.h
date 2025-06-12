// matth-x/MicroOcpp
// Copyright Matthias Akstaller 2019 - 2024
// MIT License

#ifndef MO_REQUESTSTOPTRANSACTION_H
#define MO_REQUESTSTOPTRANSACTION_H
#include <MicroOcpp/Core/Operation.h>
#include <MicroOcpp/Core/Memory.h>
#include <MicroOcpp/Model/RemoteControl/RemoteControlDefs.h>
#include <MicroOcpp/Version.h>

#if MO_ENABLE_V201

namespace MicroOcpp {

class RemoteControlService;

namespace Ocpp201 {

class RequestStopTransaction : public Operation, public MemoryManaged {
private:
    RemoteControlService& rcService;

    RequestStartStopStatus status;

    const char *errorCode = nullptr;
public:
    RequestStopTransaction(RemoteControlService& rcService);

    const char* getOperationType() override;

    void processReq(JsonObject payload) override;

    std::unique_ptr<JsonDoc> createConf() override;

    const char *getErrorCode() override {return errorCode;}

};

} //namespace Ocpp201
} //namespace MicroOcpp
#endif //MO_ENABLE_V201
#endif
