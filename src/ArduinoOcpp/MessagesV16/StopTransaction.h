// matth-x/ArduinoOcpp
// Copyright Matthias Akstaller 2019 - 2022
// MIT License

#ifndef STOPTRANSACTION_H
#define STOPTRANSACTION_H

#include <ArduinoOcpp/Core/OcppMessage.h>
#include <ArduinoOcpp/Core/OcppTime.h>
#include <ArduinoOcpp/MessagesV16/CiStrings.h>
#include <vector>

namespace ArduinoOcpp {

class SampledValue;
class MeterValue;

class Transaction;
class TransactionRPC;

namespace Ocpp16 {

class StopTransaction : public OcppMessage {
private:
    std::shared_ptr<Transaction> transaction;
    std::vector<std::unique_ptr<MeterValue>> transactionData;
public:

    StopTransaction(std::shared_ptr<Transaction> transaction);

    StopTransaction(std::shared_ptr<Transaction> transaction, std::vector<std::unique_ptr<ArduinoOcpp::MeterValue>> transactionData);
    
    StopTransaction(); //for debugging only. Make this for the server pendant

    const char* getOcppOperationType() override;

    void initiate() override;
    bool initiate(StoredOperationHandler *opStore) override;

    bool restore(StoredOperationHandler *opStore) override;

    std::unique_ptr<DynamicJsonDocument> createReq() override;

    void processConf(JsonObject payload) override;

    bool processErr(const char *code, const char *description, JsonObject details) override;

    void processReq(JsonObject payload) override;

    std::unique_ptr<DynamicJsonDocument> createConf() override;
};

} //end namespace Ocpp16
} //end namespace ArduinoOcpp
#endif
