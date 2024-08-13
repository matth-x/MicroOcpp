// matth-x/MicroOcpp
// Copyright Matthias Akstaller 2019 - 2024
// MIT License

#ifndef MO_METERVALUES_H
#define MO_METERVALUES_H

#include <MicroOcpp/Core/Operation.h>
#include <MicroOcpp/Core/Memory.h>
#include <MicroOcpp/Core/Time.h>

namespace MicroOcpp {

class MeterValue;
class Transaction;

namespace Ocpp16 {

class MeterValues : public Operation, public MemoryManaged {
private:
    Vector<std::unique_ptr<MeterValue>> meterValue;

    unsigned int connectorId = 0;

    std::shared_ptr<Transaction> transaction;

public:
    MeterValues(Vector<std::unique_ptr<MeterValue>>&& meterValue, unsigned int connectorId, std::shared_ptr<Transaction> transaction = nullptr);

    MeterValues(); //for debugging only. Make this for the server pendant

    ~MeterValues();

    const char* getOperationType() override;

    std::unique_ptr<JsonDoc> createReq() override;

    void processConf(JsonObject payload) override;

    void processReq(JsonObject payload) override;

    std::unique_ptr<JsonDoc> createConf() override;
};

} //end namespace Ocpp16
} //end namespace MicroOcpp
#endif
