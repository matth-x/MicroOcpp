// matth-x/MicroOcpp
// Copyright Matthias Akstaller 2019 - 2024
// MIT License

#ifndef MO_STARTTRANSACTION_H
#define MO_STARTTRANSACTION_H

#include <MicroOcpp/Core/Operation.h>
#include <MicroOcpp/Core/Time.h>
#include <MicroOcpp/Version.h>

#if MO_ENABLE_V16

namespace MicroOcpp {

class Context;

namespace Ocpp16 {

class Transaction;

class StartTransaction : public Operation, public MemoryManaged {
private:
    Context& context;
    Transaction *transaction = nullptr;
public:

    StartTransaction(Context& context, Transaction *transaction);

    ~StartTransaction();

    const char* getOperationType() override;

    std::unique_ptr<JsonDoc> createReq() override;

    void processConf(JsonObject payload) override;

#if MO_ENABLE_MOCK_SERVER
    static int writeMockConf(const char *operationType, char *buf, size_t size, int userStatus, void *userData);
#endif
};

} //namespace Ocpp16
} //namespace MicroOcpp
#endif //MO_ENABLE_V16
#endif
