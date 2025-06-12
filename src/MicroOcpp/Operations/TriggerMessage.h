// matth-x/MicroOcpp
// Copyright Matthias Akstaller 2019 - 2024
// MIT License

#ifndef MO_TRIGGERMESSAGE_H
#define MO_TRIGGERMESSAGE_H

#include <MicroOcpp/Core/Operation.h>
#include <MicroOcpp/Model/RemoteControl/RemoteControlDefs.h>
#include <MicroOcpp/Core/Memory.h>
#include <MicroOcpp/Version.h>

#if MO_ENABLE_V16 || MO_ENABLE_V201

namespace MicroOcpp {

class Context;
class RemoteControlService;

class TriggerMessage : public Operation, public MemoryManaged {
private:
    Context& context;
    RemoteControlService& rcService;
    TriggerMessageStatus status = TriggerMessageStatus::Rejected;

    const char *errorCode = nullptr;
public:
    TriggerMessage(Context& context, RemoteControlService& rcService);

    const char* getOperationType() override;

    void processReq(JsonObject payload) override;

    std::unique_ptr<JsonDoc> createConf() override;

    const char *getErrorCode() override {return errorCode;}
};

} //namespace MicroOcpp
#endif //MO_ENABLE_V16 || MO_ENABLE_V201
#endif
