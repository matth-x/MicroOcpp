// matth-x/MicroOcpp
// Copyright Matthias Akstaller 2019 - 2024
// MIT License

#ifndef MO_OPERATIONREGISTRY_H
#define MO_OPERATIONREGISTRY_H

#include <functional>
#include <memory>
#include <MicroOcpp/Core/Memory.h>
#include <MicroOcpp/Core/RequestCallbacks.h>

namespace MicroOcpp {

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
    Vector<OperationCreator> registry;
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
