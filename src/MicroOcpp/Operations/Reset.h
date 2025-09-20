// matth-x/MicroOcpp
// Copyright Matthias Akstaller 2019 - 2024
// MIT License

#ifndef MO_RESET_H
#define MO_RESET_H

#include <MicroOcpp/Version.h>
#include <MicroOcpp/Core/Operation.h>
#include <MicroOcpp/Model/Reset/ResetDefs.h>
#include <MicroOcpp/Version.h>

#if MO_ENABLE_V16

namespace MicroOcpp {
namespace v16 {

class ResetService;

class Reset : public Operation, public MemoryManaged {
private:
    ResetService& resetService;
    bool resetAccepted {false};
public:
    Reset(ResetService& resetService);

    const char* getOperationType() override;

    void processReq(JsonObject payload) override;

    std::unique_ptr<JsonDoc> createConf() override;
};

} //namespace v16
} //namespace MicroOcpp
#endif //MO_ENABLE_V16

#if MO_ENABLE_V201

namespace MicroOcpp {
namespace v201 {

class ResetService;

class Reset : public Operation, public MemoryManaged {
private:
    ResetService& resetService;
    MO_ResetStatus status;
    const char *errorCode = nullptr;
public:
    Reset(ResetService& resetService);

    const char* getOperationType() override;

    void processReq(JsonObject payload) override;

    std::unique_ptr<JsonDoc> createConf() override;

    const char *getErrorCode() override {return errorCode;}
};

} //namespace v201
} //namespace MicroOcpp
#endif //MO_ENABLE_V201
#endif
