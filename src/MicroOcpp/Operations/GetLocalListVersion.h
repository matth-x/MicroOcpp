// matth-x/MicroOcpp
// Copyright Matthias Akstaller 2019 - 2024
// MIT License

#ifndef MO_GETLOCALLISTVERSION_H
#define MO_GETLOCALLISTVERSION_H

#include <MicroOcpp/Core/Operation.h>
#include <MicroOcpp/Version.h>

#if MO_ENABLE_V16 && MO_ENABLE_LOCAL_AUTH

namespace MicroOcpp {
namespace Ocpp16 {

class Model;

class GetLocalListVersion : public Operation, public MemoryManaged {
private:
    Model& model;
public:
    GetLocalListVersion(Model& model);

    const char* getOperationType() override;

    void processReq(JsonObject payload) override;

    std::unique_ptr<JsonDoc> createConf() override;
};

} //namespace Ocpp16
} //namespace MicroOcpp
#endif //MO_ENABLE_V16 && MO_ENABLE_LOCAL_AUTH
#endif
