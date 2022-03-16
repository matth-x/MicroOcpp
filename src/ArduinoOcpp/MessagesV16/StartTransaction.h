// matth-x/ArduinoOcpp
// Copyright Matthias Akstaller 2019 - 2022
// MIT License

#ifndef STARTTRANSACTION_H
#define STARTTRANSACTION_H

#include <ArduinoOcpp/Core/OcppMessage.h>
#include <ArduinoOcpp/Core/OcppTime.h>
#include <ArduinoOcpp/MessagesV16/CiStrings.h>

namespace ArduinoOcpp {
namespace Ocpp16 {

class StartTransaction : public OcppMessage {
private:
    int connectorId = 1;
    int32_t meterStart = -1;
    OcppTimestamp otimestamp;
    char idTag [IDTAG_LEN_MAX + 1] = {'\0'};
    uint16_t transactionRev = 0;
public:
    StartTransaction(int connectorId);

    StartTransaction(int connectorId, const char *idTag);

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
