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

namespace Ocpp16 {

class StopTransaction : public OcppMessage {
private:
    int connectorId = 1;
    std::unique_ptr<SampledValue> meterStop {nullptr};
    OcppTimestamp otimestamp;
    char reason [REASON_LEN_MAX] {'\0'};
    std::vector<std::unique_ptr<MeterValue>> transactionData;
public:

    StopTransaction(int connectorId, const char *reason = nullptr);

    const char* getOcppOperationType();

    void initiate();

    std::unique_ptr<DynamicJsonDocument> createReq();

    void processConf(JsonObject payload);

    void processReq(JsonObject payload);

    std::unique_ptr<DynamicJsonDocument> createConf();
};

} //end namespace Ocpp16
} //end namespace ArduinoOcpp
#endif
