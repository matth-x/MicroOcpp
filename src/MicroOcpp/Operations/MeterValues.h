// matth-x/MicroOcpp
// Copyright Matthias Akstaller 2019 - 2024
// MIT License

#ifndef MO_METERVALUES_H
#define MO_METERVALUES_H

#include <MicroOcpp/Core/Operation.h>
#include <MicroOcpp/Core/Memory.h>
#include <MicroOcpp/Core/Time.h>

namespace MicroOcpp {

class Model;
class MeterValue;
class Transaction;

namespace Ocpp16 {

class MeterValues : public Operation, public MemoryManaged {
private:
    Model& model; //for adjusting the timestamp if MeterValue has been created before BootNotification
    std::unique_ptr<MeterValue> meterValue;

    unsigned int connectorId = 0;

    std::shared_ptr<Transaction> transaction;

public:
    MeterValues(Model& model, std::unique_ptr<MeterValue> meterValue, unsigned int connectorId, std::shared_ptr<Transaction> transaction = nullptr);

    MeterValues(Model& model); //for debugging only. Make this for the server pendant

    ~MeterValues();

    const char* getOperationType() override;

    std::unique_ptr<JsonDoc> createReq() override;

    void processConf(JsonObject payload) override;

    std::shared_ptr<Transaction>& getTransaction();
    unsigned int getOpNr();

    void processReq(JsonObject payload) override;

    std::unique_ptr<JsonDoc> createConf() override;
};

} //end namespace Ocpp16
} //end namespace MicroOcpp
#endif
