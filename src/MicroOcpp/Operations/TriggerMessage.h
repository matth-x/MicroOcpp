// matth-x/MicroOcpp
// Copyright Matthias Akstaller 2019 - 2024
// MIT License

#ifndef MO_TRIGGERMESSAGE_H
#define MO_TRIGGERMESSAGE_H

#include <MicroOcpp/Core/Operation.h>

#include <vector>

namespace MicroOcpp {

class Context;

namespace Ocpp16 {

class TriggerMessage : public Operation, public MemoryManaged {
private:
    Context& context;
    const char *statusMessage = nullptr;

    const char *errorCode = nullptr;
public:
    TriggerMessage(Context& context);

    const char* getOperationType() override;

    void processReq(JsonObject payload) override;

    std::unique_ptr<JsonDoc> createConf() override;

    const char *getErrorCode() override {return errorCode;}
};

} //end namespace Ocpp16
} //end namespace MicroOcpp
#endif
