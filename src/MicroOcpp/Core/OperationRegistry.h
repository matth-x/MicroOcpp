// matth-x/ArduinoOcpp
// Copyright Matthias Akstaller 2019 - 2023
// MIT License

#ifndef AO_OPERATIONREGISTRY_H
#define AO_OPERATIONREGISTRY_H

#include <functional>
#include <vector>
#include <memory>
#include <ArduinoJson.h>
#include <ArduinoOcpp/Core/RequestCallbacks.h>

namespace ArduinoOcpp {

class Operation;
class Request;

struct OperationCreator {
    const char *operationType {nullptr};
    std::function<Operation*()> creator {nullptr};
    OnReceiveReqListener onRequest {nullptr};
    OnSendConfListener onResponse {nullptr};
};

class OperationRegistry {
private:
    std::vector<OperationCreator> registry;
    OperationCreator *findCreator(const char *operationType);

public:
    OperationRegistry();

    void registerOperation(const char *operationType, std::function<Operation*()> creator);
    void setOnRequest(const char *operationType, OnReceiveReqListener onRequest);
    void setOnResponse(const char *operationType, OnSendConfListener onResponse);
    
    std::unique_ptr<Request> deserializeOperation(const char *operationType);

    void debugPrint();
};

}

#endif
