// matth-x/MicroOcpp
// Copyright Matthias Akstaller 2019 - 2024
// MIT License

#ifndef MO_OCPPERROR_H
#define MO_OCPPERROR_H

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
    std::unique_ptr<MemJsonDoc> getErrorDetails() override {
        auto errDoc = makeMemJsonDoc(JSON_OBJECT_SIZE(2), "CallError");
        JsonObject err = errDoc->to<JsonObject>();
        err["max_capacity"] = maxCapacity;
        err["msg_length"] = msgLen;
        return errDoc;
    }
};

} //end namespace MicroOcpp
#endif
