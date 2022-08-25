// matth-x/ArduinoOcpp
// Copyright Matthias Akstaller 2019 - 2022
// MIT License

#ifndef STOPTRANSACTION_H
#define STOPTRANSACTION_H

#include <ArduinoOcpp/Core/OcppMessage.h>
#include <ArduinoOcpp/Core/OcppTime.h>
#include <ArduinoOcpp/MessagesV16/CiStrings.h>

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
    ulong emTimeout = 0;
public:

    //StopTransaction(int connectorId, const char *reason = nullptr);

    StopTransaction(std::shared_ptr<Transaction> transaction);

    StopTransaction(std::shared_ptr<Transaction> transaction, std::vector<std::unique_ptr<ArduinoOcpp::MeterValue>> transactionData);
    
    StopTransaction(); //for debugging only. Make this for the server pendant

    const char* getOcppOperationType();

    void initiate();

    std::unique_ptr<DynamicJsonDocument> createReq();

    void processConf(JsonObject payload);

    bool processErr(const char *code, const char *description, JsonObject details) { return false;}

    TransactionRPC *getTransactionSync() override;

    void processReq(JsonObject payload);

    std::unique_ptr<DynamicJsonDocument> createConf();
};

} //end namespace Ocpp16
} //end namespace ArduinoOcpp
#endif
