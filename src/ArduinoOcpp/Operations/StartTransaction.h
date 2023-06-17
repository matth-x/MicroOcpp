// matth-x/ArduinoOcpp
// Copyright Matthias Akstaller 2019 - 2023
// MIT License

#ifndef STARTTRANSACTION_H
#define STARTTRANSACTION_H

#include <ArduinoOcpp/Core/Operation.h>
#include <ArduinoOcpp/Core/Time.h>
#include <ArduinoOcpp/Operations/CiStrings.h>
#include <ArduinoOcpp/Model/Metering/SampledValue.h>

namespace ArduinoOcpp {

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
} //end namespace ArduinoOcpp
#endif
