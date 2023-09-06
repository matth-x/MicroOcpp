// matth-x/MicroOcpp
// Copyright Matthias Akstaller 2019 - 2023
// MIT License

#ifndef CHANGEAVAILABILITY_H
#define CHANGEAVAILABILITY_H

#include <MicroOcpp/Core/Operation.h>

namespace MicroOcpp {

class Model;

namespace Ocpp16 {

class ChangeAvailability : public Operation {
private:
    Model& model;
    bool scheduled = false;
    bool accepted = false;

    const char *errorCode {nullptr};
public:
    ChangeAvailability(Model& model);

    const char* getOperationType() override;

    void processReq(JsonObject payload) override;

    std::unique_ptr<DynamicJsonDocument> createConf() override;

    const char *getErrorCode() override {return errorCode;}
};

} //end namespace Ocpp16
} //end namespace MicroOcpp
#endif
