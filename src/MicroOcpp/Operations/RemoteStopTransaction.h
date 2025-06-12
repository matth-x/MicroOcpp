// matth-x/MicroOcpp
// Copyright Matthias Akstaller 2019 - 2024
// MIT License

#ifndef MO_REMOTESTOPTRANSACTION_H
#define MO_REMOTESTOPTRANSACTION_H

#include <MicroOcpp/Core/Operation.h>
#include <MicroOcpp/Model/RemoteControl/RemoteControlDefs.h>
#include <MicroOcpp/Version.h>

#if MO_ENABLE_V16

namespace MicroOcpp {

class Context;
class RemoteControlService;

namespace Ocpp16 {

class RemoteStopTransaction : public Operation, public MemoryManaged {
private:
    Context& context;
    RemoteControlService& rcService;

    RemoteStartStopStatus status = RemoteStartStopStatus::Rejected;

    const char *errorCode = nullptr;
public:
    RemoteStopTransaction(Context& context, RemoteControlService& rcService);

    const char* getOperationType() override;

    void processReq(JsonObject payload) override;

    std::unique_ptr<JsonDoc> createConf() override;

    const char *getErrorCode() override {return errorCode;}
};

} //namespace Ocpp16
} //namespace MicroOcpp
#endif //MO_ENABLE_V16
#endif
