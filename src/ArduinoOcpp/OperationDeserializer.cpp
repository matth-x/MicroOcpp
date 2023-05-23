// matth-x/ArduinoOcpp
// Copyright Matthias Akstaller 2019 - 2023
// MIT License

#include <ArduinoOcpp/OperationDeserializer.h>
#include <ArduinoOcpp/Core/OcppMessage.h>
#include <ArduinoOcpp/Core/OcppOperation.h>
#include <ArduinoOcpp/Core/OcppError.h>
#include <ArduinoOcpp/Debug.h>
#include <algorithm>

using namespace ArduinoOcpp;

OperationDeserializer::OperationDeserializer() {

}

OcppMessageCreator *OperationDeserializer::findCreator(const char *operationType) {
    for (auto it = registry.begin(); it != registry.end(); ++it) {
        if (!strcmp(it->operationType, operationType)) {
            return &*it;
        }
    }
    return nullptr;
}

void OperationDeserializer::registerOcppOperation(const char *operationType, std::function<OcppMessage*()> creator) {
    registry.erase(std::remove_if(registry.begin(), registry.end(),
                [operationType] (const OcppMessageCreator& el) {
                    return !strcmp(operationType, el.operationType);
                }),
            registry.end());
    
    OcppMessageCreator entry;
    entry.operationType = operationType;
    entry.creator = creator;

    registry.push_back(entry);

    AO_DBG_DEBUG("registered operation %s", operationType);
}

void OperationDeserializer::setOnRequest(const char *operationType, OnReceiveReqListener onRequest) {
    if (auto entry = findCreator(operationType)) {
        entry->onRequest = onRequest;
    } else {
        AO_DBG_ERR("%s not registered", operationType);
    }
}

void OperationDeserializer::setOnResponse(const char *operationType, OnSendConfListener onResponse) {
    if (auto entry = findCreator(operationType)) {
        entry->onResponse = onResponse;
    } else {
        AO_DBG_ERR("%s not registered", operationType);
    }
}

std::unique_ptr<OcppOperation> OperationDeserializer::deserializeOperation(const char *operationType) {
    
    if (auto entry = findCreator(operationType)) {
        auto payload = entry->creator();
        if (payload) {
            auto result = std::unique_ptr<OcppOperation>(new OcppOperation(
                                std::unique_ptr<OcppMessage>(payload)));
            result->setOnReceiveReqListener(entry->onRequest);
            result->setOnSendConfListener(entry->onResponse);
            return result;
        }
    }

    return std::unique_ptr<OcppOperation>(new OcppOperation(
                std::unique_ptr<OcppMessage>(new NotImplemented())));
}

void OperationDeserializer::debugPrint() {
    for (auto& creator : registry) {
        AO_CONSOLE_PRINTF("[AO]     > %s\n", creator.operationType);
    }
}
