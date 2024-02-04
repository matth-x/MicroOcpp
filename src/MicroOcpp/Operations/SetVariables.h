// matth-x/MicroOcpp
// Copyright Matthias Akstaller 2019 - 2024
// MIT License

#ifndef MO_SETVARIABLES_H
#define MO_SETVARIABLES_H

#include <MicroOcpp/Version.h>

#if MO_ENABLE_V201

#include <MicroOcpp/Core/Operation.h>

#include <MicroOcpp/Model/Variables/Variable.h>

namespace MicroOcpp {

namespace Ocpp201 {

class SetVariables : public Operation {
private:
    const char *attributeType = nullptr;
    const char *attributeStatus = nullptr;
    Variable *variable = nullptr; //contains ptr to `component`

    const char *errorCode = nullptr;
public:
    SetVariables();

    const char* getOperationType() override;

    void processReq(JsonObject payload) override;

    std::unique_ptr<DynamicJsonDocument> createConf() override;

    const char *getErrorCode() override {return errorCode;}

};

} //namespace Ocpp201
} //namespace MicroOcpp

#endif //MO_ENABLE_V201

#endif
