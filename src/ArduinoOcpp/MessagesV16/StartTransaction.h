// matth-x/ArduinoOcpp
// Copyright Matthias Akstaller 2019 - 2022
// MIT License

#ifndef STARTTRANSACTION_H
#define STARTTRANSACTION_H

#include <ArduinoOcpp/Core/OcppMessage.h>
#include <ArduinoOcpp/Core/OcppTime.h>
#include <ArduinoOcpp/MessagesV16/CiStrings.h>
#include <ArduinoOcpp/Tasks/Metering/SampledValue.h>

namespace ArduinoOcpp {

class Transaction;
class TransactionRPC;
class StoredOperationHandler;

namespace Ocpp16 {

class StartTransaction : public OcppMessage {
private:
    std::shared_ptr<Transaction> transaction;
public:

    StartTransaction(std::shared_ptr<Transaction> transaction);

    StartTransaction() = default; //for debugging only. Make this for the server pendant

    const char* getOcppOperationType();

    void initiate();
    bool initiate(StoredOperationHandler *opStore) override;

    bool restore(StoredOperationHandler *opStore) override;

    std::unique_ptr<DynamicJsonDocument> createReq();

    void processConf(JsonObject payload);

    TransactionRPC *getTransactionSync() override;

    void processReq(JsonObject payload);

    std::unique_ptr<DynamicJsonDocument> createConf();
};

} //end namespace Ocpp16
} //end namespace ArduinoOcpp
#endif
