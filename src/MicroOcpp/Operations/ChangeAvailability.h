// matth-x/MicroOcpp
// Copyright Matthias Akstaller 2019 - 2024
// MIT License

#ifndef MO_CHANGEAVAILABILITY_H
#define MO_CHANGEAVAILABILITY_H

#include <MicroOcpp/Core/Operation.h>

namespace MicroOcpp {

class Model;

namespace Ocpp16 {

class ChangeAvailability : public Operation, public MemoryManaged {
private:
    Model& model;
    bool scheduled = false;
    bool accepted = false;

    const char *errorCode {nullptr};
public:
    ChangeAvailability(Model& model);

    const char* getOperationType() override;

    void processReq(JsonObject payload) override;

    std::unique_ptr<JsonDoc> createConf() override;

    const char *getErrorCode() override {return errorCode;}
};

} //end namespace Ocpp16
} //end namespace MicroOcpp

#if MO_ENABLE_V201

#include <MicroOcpp/Model/Availability/ChangeAvailabilityStatus.h>
#include <MicroOcpp/Model/Authorization/IdToken.h>

namespace MicroOcpp {

class AvailabilityService;

namespace Ocpp201 {

class ChangeAvailability : public Operation, public MemoryManaged {
private:
    AvailabilityService& availabilityService;
    ChangeAvailabilityStatus status = ChangeAvailabilityStatus::Rejected;

    const char *errorCode {nullptr};
public:
    ChangeAvailability(AvailabilityService& availabilityService);

    const char* getOperationType() override;

    void processReq(JsonObject payload) override;

    std::unique_ptr<JsonDoc> createConf() override;

    const char *getErrorCode() override {return errorCode;}
};

} //end namespace Ocpp201
} //end namespace MicroOcpp

#endif //MO_ENABLE_V201

#endif
