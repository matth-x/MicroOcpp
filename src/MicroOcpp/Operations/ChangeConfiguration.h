// matth-x/MicroOcpp
// Copyright Matthias Akstaller 2019 - 2023
// MIT License

#ifndef CHANGECONFIGURATION_H
#define CHANGECONFIGURATION_H

#include <MicroOcpp/Core/Operation.h>

namespace MicroOcpp {
namespace Ocpp16 {

class ChangeConfiguration : public Operation {
private:
    bool reject = false;
    bool rebootRequired = false;
    bool readOnly = false;
    bool notSupported = false;

    const char *errorCode = nullptr;
public:
    ChangeConfiguration();

    const char* getOperationType() override;

    void processReq(JsonObject payload) override;

    std::unique_ptr<DynamicJsonDocument> createConf() override;

    const char *getErrorCode() override {return errorCode;}

};

} //end namespace Ocpp16
} //end namespace MicroOcpp
#endif
