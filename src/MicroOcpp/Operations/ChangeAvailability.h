// matth-x/MicroOcpp
// Copyright Matthias Akstaller 2019 - 2025
// MIT License

#ifndef MO_CHANGEAVAILABILITY_H
#define MO_CHANGEAVAILABILITY_H

#include <MicroOcpp/Core/Operation.h>
#include <MicroOcpp/Version.h>

#if MO_ENABLE_V16

namespace MicroOcpp {
namespace v16 {

class Model;

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

} //namespace v16
} //namespace MicroOcpp

#endif //MO_ENABLE_V16

#if MO_ENABLE_V201

#include <MicroOcpp/Model/Availability/AvailabilityDefs.h>
#include <MicroOcpp/Model/Authorization/IdToken.h>

namespace MicroOcpp {
namespace v201 {

class AvailabilityService;

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

} //namespace v201
} //namespace MicroOcpp

#endif //MO_ENABLE_V201

#endif
