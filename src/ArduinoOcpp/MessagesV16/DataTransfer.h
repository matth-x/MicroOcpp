// matth-x/ArduinoOcpp
// Copyright Matthias Akstaller 2019 - 2022
// MIT License

#ifndef DATATRANSFER_H
#define DATATRANSFER_H

#include <ArduinoOcpp/Core/OcppMessage.h>

namespace ArduinoOcpp {
namespace Ocpp16 {

class DataTransfer : public OcppMessage {
private:
  String msg;
public:
    DataTransfer(String &msg);

    const char* getOcppOperationType();

    std::unique_ptr<DynamicJsonDocument> createReq();

    void processConf(JsonObject payload);
    
};

} //end namespace Ocpp16
} //end namespace ArduinoOcpp
#endif
