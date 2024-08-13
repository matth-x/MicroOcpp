// matth-x/MicroOcpp
// Copyright Matthias Akstaller 2019 - 2024
// MIT License

#ifndef MO_OCPPERROR_H
#define MO_OCPPERROR_H

#include <MicroOcpp/Core/Operation.h>
#include <MicroOcpp/Core/Memory.h>

namespace MicroOcpp {

class NotImplemented : public Operation, public MemoryManaged {
public:
    NotImplemented() : MemoryManaged("v16.CallError.", "NotImplemented") { }

    const char *getErrorCode() override {
        return "NotImplemented";
    }
};

class MsgBufferExceeded : public Operation, public MemoryManaged {
private:
    size_t maxCapacity;
    size_t msgLen;
public:
    MsgBufferExceeded(size_t maxCapacity, size_t msgLen) : MemoryManaged("v16.CallError.", "GenericError"), maxCapacity(maxCapacity), msgLen(msgLen) { }
    const char *getErrorCode() override {
        return "GenericError";
    }
    const char *getErrorDescription() override {
        return "JSON too long or too many fields. Cannot deserialize";
    }
    std::unique_ptr<JsonDoc> getErrorDetails() override {
        auto errDoc = makeJsonDoc(getMemoryTag(), JSON_OBJECT_SIZE(2));
        JsonObject err = errDoc->to<JsonObject>();
        err["max_capacity"] = maxCapacity;
        err["msg_length"] = msgLen;
        return errDoc;
    }
};

} //end namespace MicroOcpp
#endif
