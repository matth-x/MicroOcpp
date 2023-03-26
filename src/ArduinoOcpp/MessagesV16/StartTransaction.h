// matth-x/ArduinoOcpp
// Copyright Matthias Akstaller 2019 - 2023
// MIT License

#ifndef STARTTRANSACTION_H
#define STARTTRANSACTION_H

#include <ArduinoOcpp/Core/OcppMessage.h>
#include <ArduinoOcpp/Core/OcppTime.h>
#include <ArduinoOcpp/MessagesV16/CiStrings.h>
#include <ArduinoOcpp/Tasks/Metering/SampledValue.h>

namespace ArduinoOcpp {

class OcppModel;
class Transaction;

namespace Ocpp16 {

class StartTransaction : public OcppMessage {
private:
    OcppModel& context;
    std::shared_ptr<Transaction> transaction;
public:

    StartTransaction(OcppModel& context, std::shared_ptr<Transaction> transaction);

    ~StartTransaction();

    const char* getOcppOperationType();

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
