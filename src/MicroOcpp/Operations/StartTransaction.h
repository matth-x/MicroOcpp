// matth-x/MicroOcpp
// Copyright Matthias Akstaller 2019 - 2024
// MIT License

#ifndef MO_STARTTRANSACTION_H
#define MO_STARTTRANSACTION_H

#include <MicroOcpp/Core/Operation.h>
#include <MicroOcpp/Core/Time.h>
#include <MicroOcpp/Operations/CiStrings.h>
#include <MicroOcpp/Model/Metering/SampledValue.h>

namespace MicroOcpp {

class Model;
class Transaction;

namespace Ocpp16 {

class StartTransaction : public Operation, public MemoryManaged {
private:
    Model& model;
    std::shared_ptr<Transaction> transaction;
public:

    StartTransaction(Model& model, std::shared_ptr<Transaction> transaction);

    ~StartTransaction();

    const char* getOperationType() override;

    std::unique_ptr<JsonDoc> createReq() override;

    void processConf(JsonObject payload) override;

    void processReq(JsonObject payload) override;

    std::unique_ptr<JsonDoc> createConf() override;
};

} //end namespace Ocpp16
} //end namespace MicroOcpp
#endif
