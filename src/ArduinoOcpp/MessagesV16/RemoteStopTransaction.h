// matth-x/ArduinoOcpp
// Copyright Matthias Akstaller 2019 - 2022
// MIT License

#ifndef REMOTESTOPTRANSACTION_H
#define REMOTESTOPTRANSACTION_H

#include <ArduinoOcpp/Core/OcppMessage.h>

#include <Variants.h>

namespace ArduinoOcpp {
namespace Ocpp16 {

class RemoteStopTransaction : public OcppMessage {
private:
    int transactionId;
public:
    RemoteStopTransaction();

    const char* getOcppOperationType();

    void processReq(JsonObject payload);

    std::unique_ptr<DynamicJsonDocument> createConf();
};

} //end namespace Ocpp16
} //end namespace ArduinoOcpp
#endif
