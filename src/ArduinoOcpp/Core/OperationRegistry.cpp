// matth-x/ArduinoOcpp
// Copyright Matthias Akstaller 2019 - 2023
// MIT License

#include <ArduinoOcpp/Core/OperationRegistry.h>
#include <ArduinoOcpp/Core/Operation.h>
#include <ArduinoOcpp/Core/Request.h>
#include <ArduinoOcpp/Core/OcppError.h>
#include <ArduinoOcpp/Debug.h>
#include <algorithm>

using namespace ArduinoOcpp;

OperationRegistry::OperationRegistry() {

}

OperationCreator *OperationRegistry::findCreator(const char *operationType) {
    for (auto it = registry.begin(); it != registry.end(); ++it) {
        if (!strcmp(it->operationType, operationType)) {
            return &*it;
        }
    }
    return nullptr;
}

void OperationRegistry::registerRequest(const char *operationType, std::function<Operation*()> creator) {
    registry.erase(std::remove_if(registry.begin(), registry.end(),
                [operationType] (const OperationCreator& el) {
                    return !strcmp(operationType, el.operationType);
                }),
            registry.end());
    
    OperationCreator entry;
    entry.operationType = operationType;
    entry.creator = creator;

    registry.push_back(entry);

    AO_DBG_DEBUG("registered operation %s", operationType);
}

void OperationRegistry::setOnRequest(const char *operationType, OnReceiveReqListener onRequest) {
    if (auto entry = findCreator(operationType)) {
        entry->onRequest = onRequest;
    } else {
        AO_DBG_ERR("%s not registered", operationType);
    }
}

void OperationRegistry::setOnResponse(const char *operationType, OnSendConfListener onResponse) {
    if (auto entry = findCreator(operationType)) {
        entry->onResponse = onResponse;
    } else {
        AO_DBG_ERR("%s not registered", operationType);
    }
}

std::unique_ptr<Request> OperationRegistry::deserializeOperation(const char *operationType) {
    
    if (auto entry = findCreator(operationType)) {
        auto payload = entry->creator();
        if (payload) {
            auto result = std::unique_ptr<Request>(new Request(
                                std::unique_ptr<Operation>(payload)));
            result->setOnReceiveReqListener(entry->onRequest);
            return result;
        }
    }

    return std::unique_ptr<Request>(new Request(
                std::unique_ptr<Operation>(new NotImplemented())));
}

void OperationRegistry::debugPrint() {
    for (auto& creator : registry) {
        AO_CONSOLE_PRINTF("[AO]     > %s\n", creator.operationType);
    }
}
