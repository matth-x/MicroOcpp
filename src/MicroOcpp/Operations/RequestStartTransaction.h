// matth-x/MicroOcpp
// Copyright Matthias Akstaller 2019 - 2024
// MIT License

#ifndef MO_REQUESTSTARTTRANSACTION_H
#define MO_REQUESTSTARTTRANSACTION_H

#include <MicroOcpp/Version.h>

#if MO_ENABLE_V201

#include <string>
#include <MicroOcpp/Core/Operation.h>
#include <MicroOcpp/Model/Transactions/TransactionDefs.h>

namespace MicroOcpp {

class TransactionService;

namespace Ocpp201 {

class RequestStartTransaction : public Operation, public AllocOverrider {
private:
    TransactionService& txService;

    RequestStartStopStatus status;
    char transactionId [MO_TXID_LEN_MAX + 1] = {'\0'};

    const char *errorCode = nullptr;
public:
    RequestStartTransaction(TransactionService& txService);

    const char* getOperationType() override;

    void processReq(JsonObject payload) override;

    std::unique_ptr<MemJsonDoc> createConf() override;

    const char *getErrorCode() override {return errorCode;}

};

} //namespace Ocpp201
} //namespace MicroOcpp

#endif //MO_ENABLE_V201

#endif
