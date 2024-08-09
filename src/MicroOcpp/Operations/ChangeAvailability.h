// matth-x/MicroOcpp
// Copyright Matthias Akstaller 2019 - 2024
// MIT License

#ifndef MO_CHANGEAVAILABILITY_H
#define MO_CHANGEAVAILABILITY_H

#include <MicroOcpp/Core/Operation.h>

namespace MicroOcpp {

class Model;

namespace Ocpp16 {

class ChangeAvailability : public Operation, public AllocOverrider {
private:
    Model& model;
    bool scheduled = false;
    bool accepted = false;

    const char *errorCode {nullptr};
public:
    ChangeAvailability(Model& model);

    const char* getOperationType() override;

    void processReq(JsonObject payload) override;

    std::unique_ptr<MemJsonDoc> createConf() override;

    const char *getErrorCode() override {return errorCode;}
};

} //end namespace Ocpp16
} //end namespace MicroOcpp
#endif
