// matth-x/MicroOcpp
// Copyright Matthias Akstaller 2019 - 2024
// MIT License

#ifndef MO_TRANSACTIONEVENT_H
#define MO_TRANSACTIONEVENT_H

#include <MicroOcpp/Core/Operation.h>
#include <MicroOcpp/Core/Time.h>
#include <MicroOcpp/Core/Memory.h>
#include <MicroOcpp/Version.h>

#if MO_ENABLE_V201

namespace MicroOcpp {

class Context;

namespace v201 {

class Variable;

class NotifyReport : public Operation, public MemoryManaged {
private:
    Context& context;

    int requestId;
    Timestamp generatedAt;
    bool tbc;
    unsigned int seqNo;
    Vector<Variable*> reportData;
public:

    NotifyReport(Context& context, int requestId, const Timestamp& generatedAt, bool tbc, unsigned int seqNo, const Vector<Variable*>& reportData);

    const char* getOperationType() override;

    std::unique_ptr<JsonDoc> createReq() override;

    void processConf(JsonObject payload) override;
};

} //namespace v201
} //namespace MicroOcpp
#endif //MO_ENABLE_V201
#endif
