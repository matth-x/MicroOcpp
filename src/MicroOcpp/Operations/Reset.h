// matth-x/MicroOcpp
// Copyright Matthias Akstaller 2019 - 2024
// MIT License

#ifndef RESET_H
#define RESET_H

#include <MicroOcpp/Version.h>
#include <MicroOcpp/Core/Operation.h>
#include <MicroOcpp/Model/Reset/ResetDefs.h>

namespace MicroOcpp {

class Model;

namespace Ocpp16 {

class Reset : public Operation, public AllocOverrider {
private:
    Model& model;
    bool resetAccepted {false};
public:
    Reset(Model& model);

    const char* getOperationType() override;

    void processReq(JsonObject payload) override;

    std::unique_ptr<MemJsonDoc> createConf() override;
};

} //end namespace Ocpp16
} //end namespace MicroOcpp

#if MO_ENABLE_V201

namespace MicroOcpp {
namespace Ocpp201 {

class ResetService;

class Reset : public Operation, public AllocOverrider {
private:
    ResetService& resetService;
    ResetStatus status;
    const char *errorCode = nullptr;
public:
    Reset(ResetService& resetService);

    const char* getOperationType() override;

    void processReq(JsonObject payload) override;

    std::unique_ptr<MemJsonDoc> createConf() override;

    const char *getErrorCode() override {return errorCode;}
};

} //end namespace Ocpp201
} //end namespace MicroOcpp

#endif //MO_ENABLE_V201
#endif
