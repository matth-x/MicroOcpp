// matth-x/ArduinoOcpp
// Copyright Matthias Akstaller 2019 - 2022
// MIT License

#ifndef STARTTRANSACTION_H
#define STARTTRANSACTION_H

#include <ArduinoOcpp/Core/OcppMessage.h>
#include <ArduinoOcpp/Core/OcppTime.h>

#include <Variants.h>

namespace ArduinoOcpp {
namespace Ocpp16 {

class StartTransaction : public OcppMessage {
private:
    int connectorId = 1;
    float meterStart = -1.0f;
    OcppTimestamp otimestamp;
    String idTag = String('\0');
    uint16_t transactionRev = 0;
public:
    StartTransaction(int connectorId);

    StartTransaction(int connectorId, String &idTag);

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
