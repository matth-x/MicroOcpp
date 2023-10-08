// matth-x/MicroOcpp
// Copyright Matthias Akstaller 2019 - 2023
// MIT License

#ifndef TRIGGERMESSAGE_H
#define TRIGGERMESSAGE_H

#include <MicroOcpp/Core/Operation.h>

#include <vector>

namespace MicroOcpp {

class Context;

namespace Ocpp16 {

class TriggerMessage : public Operation {
private:
    Context& context;
    const char *statusMessage = nullptr;

    const char *errorCode = nullptr;
public:
    TriggerMessage(Context& context);

    const char* getOperationType() override;

    void processReq(JsonObject payload) override;

    std::unique_ptr<DynamicJsonDocument> createConf() override;

    const char *getErrorCode() override {return errorCode;}
};

} //end namespace Ocpp16
} //end namespace MicroOcpp
#endif
