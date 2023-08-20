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

    const char* getOperationType();

    void initiate(StoredOperationHandler *opStore) override;

    bool restore(StoredOperationHandler *opStore) override;

    std::unique_ptr<DynamicJsonDocument> createReq();

    void processConf(JsonObject payload);

    void processReq(JsonObject payload);

    std::unique_ptr<DynamicJsonDocument> createConf();
};

} //end namespace Ocpp16
} //end namespace MicroOcpp
#endif
