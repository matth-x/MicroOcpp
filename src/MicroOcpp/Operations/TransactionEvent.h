// matth-x/MicroOcpp
// Copyright Matthias Akstaller 2019 - 2024
// MIT License

#ifndef MO_TRANSACTIONEVENT_H
#define MO_TRANSACTIONEVENT_H

#include <MicroOcpp/Core/Operation.h>
#include <MicroOcpp/Version.h>

#if MO_ENABLE_V201

namespace MicroOcpp {

class Context;

namespace Ocpp201 {

class TransactionEventData;

class TransactionEvent : public Operation, public MemoryManaged {
private:
    Context& context;
    TransactionEventData *txEvent; //does not take ownership

    const char *errorCode = nullptr;
public:

    TransactionEvent(Context& context, TransactionEventData *txEvent);

    const char* getOperationType() override;

    std::unique_ptr<JsonDoc> createReq() override;

    void processConf(JsonObject payload) override;

    const char *getErrorCode() override {return errorCode;}

#if MO_ENABLE_MOCK_SERVER
    static void onRequestMock(const char *operationType, const char *payloadJson, int *userStatus, void *userData);
    static int writeMockConf(const char *operationType, char *buf, size_t size, int userStatus, void *userData);
#endif
};

} //namespace Ocpp201
} //namespace MicroOcpp
#endif //MO_ENABLE_V201
#endif
