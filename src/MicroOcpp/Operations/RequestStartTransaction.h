// matth-x/MicroOcpp
// Copyright Matthias Akstaller 2019 - 2024
// MIT License

#ifndef MO_REQUESTSTARTTRANSACTION_H
#define MO_REQUESTSTARTTRANSACTION_H

#include <MicroOcpp/Version.h>

#if MO_ENABLE_V201

#include <MicroOcpp/Core/Operation.h>
#include <MicroOcpp/Core/Memory.h>
#include <MicroOcpp/Model/RemoteControl/RemoteControlDefs.h>
#include <MicroOcpp/Model/Transactions/Transaction.h>

namespace MicroOcpp {

class RemoteControlService;

namespace Ocpp201 {

class RequestStartTransaction : public Operation, public MemoryManaged {
private:
    RemoteControlService& rcService;

    RequestStartStopStatus status;
    std::shared_ptr<Ocpp201::Transaction> transaction;

    const char *errorCode = nullptr;
public:
    RequestStartTransaction(RemoteControlService& rcService);

    const char* getOperationType() override;

    void processReq(JsonObject payload) override;

    std::unique_ptr<JsonDoc> createConf() override;

    const char *getErrorCode() override {return errorCode;}

};

} //namespace Ocpp201
} //namespace MicroOcpp

#endif //MO_ENABLE_V201

#endif
