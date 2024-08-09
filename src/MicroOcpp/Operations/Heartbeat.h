// matth-x/MicroOcpp
// Copyright Matthias Akstaller 2019 - 2024
// MIT License

#ifndef MO_HEARTBEAT_H
#define MO_HEARTBEAT_H

#include <MicroOcpp/Core/Operation.h>

namespace MicroOcpp {

class Model;

namespace Ocpp16 {

class Heartbeat : public Operation, public AllocOverrider {
private:
    Model& model;
public:
    Heartbeat(Model& model);

    const char* getOperationType() override;

    std::unique_ptr<MemJsonDoc> createReq() override;

    void processConf(JsonObject payload) override;

    void processReq(JsonObject payload) override;

    std::unique_ptr<MemJsonDoc> createConf() override;
};

} //end namespace Ocpp16
} //end namespace MicroOcpp
#endif
