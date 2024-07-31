// matth-x/MicroOcpp
// Copyright Matthias Akstaller 2019 - 2024
// MIT License

#ifndef MO_STOPTRANSACTION_H
#define MO_STOPTRANSACTION_H

#include <MicroOcpp/Core/Operation.h>
#include <MicroOcpp/Core/Time.h>
#include <MicroOcpp/Operations/CiStrings.h>
#include <vector>

namespace MicroOcpp {

class Model;

class SampledValue;
class MeterValue;

class Transaction;

namespace Ocpp16 {

class StopTransaction : public Operation {
private:
    Model& model;
    std::shared_ptr<Transaction> transaction;
    std::vector<std::unique_ptr<MeterValue>> transactionData;
public:

    StopTransaction(Model& model, std::shared_ptr<Transaction> transaction);

    StopTransaction(Model& model, std::shared_ptr<Transaction> transaction, std::vector<std::unique_ptr<MicroOcpp::MeterValue>> transactionData);

    const char* getOperationType() override;

    std::unique_ptr<DynamicJsonDocument> createReq() override;

    void processConf(JsonObject payload) override;

    bool processErr(const char *code, const char *description, JsonObject details) override;

    void processReq(JsonObject payload) override;

    std::unique_ptr<DynamicJsonDocument> createConf() override;
};

} //end namespace Ocpp16
} //end namespace MicroOcpp
#endif
