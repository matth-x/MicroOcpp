// matth-x/MicroOcpp
// Copyright Matthias Akstaller 2019 - 2023
// MIT License

#ifndef METERVALUES_H
#define METERVALUES_H

#include <MicroOcpp/Core/Operation.h>
#include <MicroOcpp/Core/Time.h>

#include <vector>

namespace MicroOcpp {

class MeterValue;
class Transaction;

namespace Ocpp16 {

class MeterValues : public Operation {
private:
    std::vector<std::unique_ptr<MeterValue>> meterValue;

    unsigned int connectorId = 0;

    std::shared_ptr<Transaction> transaction;

public:
    MeterValues(std::vector<std::unique_ptr<MeterValue>>&& meterValue, unsigned int connectorId, std::shared_ptr<Transaction> transaction = nullptr);

    MeterValues(); //for debugging only. Make this for the server pendant

    ~MeterValues();

    const char* getOperationType() override;

    std::unique_ptr<DynamicJsonDocument> createReq() override;

    void processConf(JsonObject payload) override;

    void processReq(JsonObject payload) override;

    std::unique_ptr<DynamicJsonDocument> createConf() override;
};

} //end namespace Ocpp16
} //end namespace MicroOcpp
#endif
