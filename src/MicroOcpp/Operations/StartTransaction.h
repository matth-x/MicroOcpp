// matth-x/MicroOcpp
// Copyright Matthias Akstaller 2019 - 2023
// MIT License

#ifndef STARTTRANSACTION_H
#define STARTTRANSACTION_H

#include <MicroOcpp/Core/Operation.h>
#include <MicroOcpp/Core/Time.h>
#include <MicroOcpp/Operations/CiStrings.h>
#include <MicroOcpp/Model/Metering/SampledValue.h>

namespace MicroOcpp {

class Model;
class Transaction;

namespace Ocpp16 {

class StartTransaction : public Operation {
private:
    Model& model;
    std::shared_ptr<Transaction> transaction;
public:

    StartTransaction(Model& model, std::shared_ptr<Transaction> transaction);

    ~StartTransaction();

    const char* getOperationType() override;

    void initiate(StoredOperationHandler *opStore) override;

    bool restore(StoredOperationHandler *opStore) override;

    std::unique_ptr<DynamicJsonDocument> createReq() override;

    void processConf(JsonObject payload) override;

    void processReq(JsonObject payload) override;

    std::unique_ptr<DynamicJsonDocument> createConf() override;
};

} //end namespace Ocpp16
} //end namespace MicroOcpp
#endif
