// matth-x/MicroOcpp
// Copyright Matthias Akstaller 2019 - 2024
// MIT License

#ifndef MO_REQUESTSTARTTRANSACTION_H
#define MO_REQUESTSTARTTRANSACTION_H

#include <MicroOcpp/Core/Operation.h>
#include <MicroOcpp/Core/Memory.h>
#include <MicroOcpp/Model/RemoteControl/RemoteControlDefs.h>
#include <MicroOcpp/Model/Transactions/Transaction.h>
#include <MicroOcpp/Version.h>

#if MO_ENABLE_V201

namespace MicroOcpp {

class Context;
class RemoteControlService;

namespace v201 {

class RequestStartTransaction : public Operation, public MemoryManaged {
private:
    Context& context;
    RemoteControlService& rcService;

    RequestStartStopStatus status;
    std::shared_ptr<v201::Transaction> transaction;
    char transactionId [MO_TXID_SIZE] = {'\0'};

    const char *errorCode = nullptr;
public:
    RequestStartTransaction(Context& context, RemoteControlService& rcService);

    const char* getOperationType() override;

    void processReq(JsonObject payload) override;

    std::unique_ptr<JsonDoc> createConf() override;

    const char *getErrorCode() override {return errorCode;}

};

} //namespace v201
} //namespace MicroOcpp
#endif //MO_ENABLE_V201
#endif
