// matth-x/MicroOcpp
// Copyright Matthias Akstaller 2019 - 2024
// MIT License

#ifndef MO_STOPTRANSACTION_H
#define MO_STOPTRANSACTION_H

#include <MicroOcpp/Core/Operation.h>
#include <MicroOcpp/Core/Memory.h>
#include <MicroOcpp/Core/Time.h>
#include <MicroOcpp/Version.h>

#if MO_ENABLE_V16

namespace MicroOcpp {

class Context;

namespace v16 {

class Transaction;

class StopTransaction : public Operation, public MemoryManaged {
private:
    Context& context;
    Transaction *transaction = nullptr;
public:

    StopTransaction(Context& context, Transaction *transaction);

    const char* getOperationType() override;

    std::unique_ptr<JsonDoc> createReq() override;

    void processConf(JsonObject payload) override;

#if MO_ENABLE_MOCK_SERVER
    static int writeMockConf(const char *operationType, char *buf, size_t size, void *userStatus, void *userData);
#endif
};

} //namespace v16
} //namespace MicroOcpp
#endif //MO_ENABLE_V16
#endif
