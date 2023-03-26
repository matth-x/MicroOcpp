// matth-x/ArduinoOcpp
// Copyright Matthias Akstaller 2019 - 2023
// MIT License

#ifndef OPERATIONDESERIALIZER_H
#define OPERATIONDESERIALIZER_H

#include <functional>
#include <vector>
#include <memory>
#include <ArduinoJson.h>
#include <ArduinoOcpp/Core/OcppOperationCallbacks.h>

namespace ArduinoOcpp {

class OcppMessage;
class OcppOperation;

struct OcppMessageCreator {
    const char *operationType {nullptr};
    std::function<OcppMessage*()> creator {nullptr};
    OnReceiveReqListener onRequest {nullptr};
    OnSendConfListener onResponse {nullptr};
};

class OperationDeserializer {
private:
    std::vector<OcppMessageCreator> registry;
    OcppMessageCreator *findCreator(const char *operationType);

public:
    OperationDeserializer();

    void registerOcppOperation(const char *operationType, std::function<OcppMessage*()> creator);
    void setOnRequest(const char *operationType, OnReceiveReqListener onRequest);
    void setOnResponse(const char *operationType, OnSendConfListener onResponse);
    
    std::unique_ptr<OcppOperation> deserializeOperation(const char *operationType);

    void debugPrint();
};

}

#endif
