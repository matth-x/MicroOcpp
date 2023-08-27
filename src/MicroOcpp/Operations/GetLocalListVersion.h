// matth-x/MicroOcpp
// Copyright Matthias Akstaller 2019 - 2023
// MIT License

#ifndef GETLOCALLISTVERSION_H
#define GETLOCALLISTVERSION_H

#include <MicroOcpp/Core/Operation.h>

namespace MicroOcpp {

class Model;

namespace Ocpp16 {

class GetLocalListVersion : public Operation {
private:
    Model& model;
public:
    GetLocalListVersion(Model& model);

    const char* getOperationType() override;

    void processReq(JsonObject payload) override;

    std::unique_ptr<DynamicJsonDocument> createConf() override;
};

} //end namespace Ocpp16
} //end namespace MicroOcpp
#endif
