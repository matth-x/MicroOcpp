// matth-x/MicroOcpp
// Copyright Matthias Akstaller 2019 - 2023
// MIT License

#ifndef MO_TRANSACTIONEVENT_H
#define MO_TRANSACTIONEVENT_H

#include <MicroOcpp/Version.h>

#if MO_ENABLE_V201

#include <MicroOcpp/Core/Operation.h>

namespace MicroOcpp {

class Model;

namespace Ocpp201 {

class TransactionEventData;

class TransactionEvent : public Operation {
private:
    Model& model;
    std::shared_ptr<TransactionEventData> txEvent;

    const char *errorCode = nullptr;
public:

    TransactionEvent(Model& model, std::shared_ptr<TransactionEventData> txEvent);

    const char* getOperationType() override;

    void initiate(StoredOperationHandler *opStore) override;

    std::unique_ptr<DynamicJsonDocument> createReq() override;

    void processConf(JsonObject payload) override;

    const char *getErrorCode() override {return errorCode;}

    void processReq(JsonObject payload) override;

    std::unique_ptr<DynamicJsonDocument> createConf() override;
};

} //end namespace Ocpp201
} //end namespace MicroOcpp
#endif // MO_ENABLE_V201
#endif
