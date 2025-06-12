// matth-x/MicroOcpp
// Copyright Matthias Akstaller 2019 - 2025
// MIT License

#ifndef MO_LOGSTATUSNOTIFICATION_H
#define MO_LOGSTATUSNOTIFICATION_H

#include <MicroOcpp/Core/Operation.h>
#include <MicroOcpp/Model/Diagnostics/Diagnostics.h>
#include <MicroOcpp/Version.h>

#if (MO_ENABLE_V16 || MO_ENABLE_V201) && MO_ENABLE_DIAGNOSTICS

namespace MicroOcpp {

class LogStatusNotification : public Operation, public MemoryManaged {
private:
    MO_UploadLogStatus status;
    int requestId;
public:
    LogStatusNotification(MO_UploadLogStatus status, int requestId);

    const char* getOperationType() override;

    std::unique_ptr<JsonDoc> createReq() override;

    void processConf(JsonObject payload) override;

    void processReq(JsonObject payload) override;

    std::unique_ptr<JsonDoc> createConf() override;
};

} //namespace MicroOcpp
#endif //(MO_ENABLE_V16 || MO_ENABLE_V201) && MO_ENABLE_DIAGNOSTICS
#endif
