// matth-x/MicroOcpp
// Copyright Matthias Akstaller 2019 - 2023
// MIT License

#ifndef OCPPERROR_H
#define OCPPERROR_H

#include <ArduinoJson.h>

#include <MicroOcpp/Core/Operation.h>

namespace MicroOcpp {

class NotImplemented : public Operation {
public:
    const char *getErrorCode() override {
        return "NotImplemented";
    }
};

class MsgBufferExceeded : public Operation {
private:
    size_t maxCapacity;
    size_t msgLen;
public:
    MsgBufferExceeded(size_t maxCapacity, size_t msgLen) : maxCapacity(maxCapacity), msgLen(msgLen) { }
    const char *getErrorCode() override {
        return "GenericError";
    }
    const char *getErrorDescription() override {
        return "JSON too long or too many fields. Cannot deserialize";
    }
    std::unique_ptr<DynamicJsonDocument> getErrorDetails() override {
        auto errDoc = std::unique_ptr<DynamicJsonDocument>(new DynamicJsonDocument(JSON_OBJECT_SIZE(2)));
        JsonObject err = errDoc->to<JsonObject>();
        err["max_capacity"] = maxCapacity;
        err["msg_length"] = msgLen;
        return errDoc;
    }
};

} //end namespace MicroOcpp
#endif
