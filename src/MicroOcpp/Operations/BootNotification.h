// matth-x/MicroOcpp
// Copyright Matthias Akstaller 2019 - 2024
// MIT License

#ifndef MO_BOOTNOTIFICATION_H
#define MO_BOOTNOTIFICATION_H

#include <MicroOcpp/Core/Operation.h>
#include <MicroOcpp/Operations/CiStrings.h>

#define CP_MODEL_LEN_MAX        CiString20TypeLen
#define CP_SERIALNUMBER_LEN_MAX CiString25TypeLen
#define CP_VENDOR_LEN_MAX       CiString20TypeLen
#define FW_VERSION_LEN_MAX      CiString50TypeLen

namespace MicroOcpp {

class Model;

namespace Ocpp16 {

class BootNotification : public Operation, public AllocOverrider {
private:
    Model& model;
    std::unique_ptr<MemJsonDoc> credentials;
    const char *errorCode = nullptr;
public:
    BootNotification(Model& model, std::unique_ptr<MemJsonDoc> payload);

    ~BootNotification() = default;

    const char* getOperationType() override;

    std::unique_ptr<MemJsonDoc> createReq() override;

    void processConf(JsonObject payload) override;

    void processReq(JsonObject payload) override;

    std::unique_ptr<MemJsonDoc> createConf() override;

    const char *getErrorCode() override {return errorCode;}
};

} //end namespace Ocpp16
} //end namespace MicroOcpp

#endif
