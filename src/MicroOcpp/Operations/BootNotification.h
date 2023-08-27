// matth-x/MicroOcpp
// Copyright Matthias Akstaller 2019 - 2023
// MIT License

#ifndef BOOTNOTIFICATION_H
#define BOOTNOTIFICATION_H

#include <MicroOcpp/Core/Operation.h>
#include <MicroOcpp/Operations/CiStrings.h>

#define CP_MODEL_LEN_MAX        CiString20TypeLen
#define CP_SERIALNUMBER_LEN_MAX CiString25TypeLen
#define CP_VENDOR_LEN_MAX       CiString20TypeLen
#define FW_VERSION_LEN_MAX      CiString50TypeLen

namespace MicroOcpp {

class Model;

namespace Ocpp16 {

class BootNotification : public Operation {
private:
    Model& model;
    std::unique_ptr<DynamicJsonDocument> credentials;
    const char *errorCode = nullptr;
public:
    BootNotification(Model& model, std::unique_ptr<DynamicJsonDocument> payload);

    ~BootNotification() = default;

    const char* getOperationType() override;

    std::unique_ptr<DynamicJsonDocument> createReq() override;

    void processConf(JsonObject payload) override;

    void processReq(JsonObject payload) override;

    std::unique_ptr<DynamicJsonDocument> createConf() override;

    const char *getErrorCode() override {return errorCode;}
};

} //end namespace Ocpp16
} //end namespace MicroOcpp

#endif
