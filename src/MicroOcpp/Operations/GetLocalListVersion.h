// matth-x/MicroOcpp
// Copyright Matthias Akstaller 2019 - 2024
// MIT License

#ifndef MO_GETLOCALLISTVERSION_H
#define MO_GETLOCALLISTVERSION_H

#include <MicroOcpp/Version.h>

#if MO_ENABLE_LOCAL_AUTH

#include <MicroOcpp/Core/Operation.h>

namespace MicroOcpp {

class Model;

namespace Ocpp16 {

class GetLocalListVersion : public Operation, public MemoryManaged {
private:
    Model& model;
public:
    GetLocalListVersion(Model& model);

    const char* getOperationType() override;

    void processReq(JsonObject payload) override;

    std::unique_ptr<JsonDoc> createConf() override;
};

} //end namespace Ocpp16
} //end namespace MicroOcpp

#endif //MO_ENABLE_LOCAL_AUTH
#endif
