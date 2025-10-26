// matth-x/MicroOcpp
// Copyright Matthias Akstaller 2019 - 2024
// MIT License

#ifndef MO_CHANGECONFIGURATION_H
#define MO_CHANGECONFIGURATION_H

#include <MicroOcpp/Core/Operation.h>
#include <MicroOcpp/Model/Configuration/Configuration.h>
#include <MicroOcpp/Version.h>

#if MO_ENABLE_V16

namespace MicroOcpp {
namespace v16 {

class ConfigurationService;

class ChangeConfiguration : public Operation, public MemoryManaged {
private:
    ConfigurationService& configService;
    ConfigurationStatus status = ConfigurationStatus::Rejected;

    const char *errorCode = nullptr;
public:
    ChangeConfiguration(ConfigurationService& configService);

    const char* getOperationType() override;

    void processReq(JsonObject payload) override;

    std::unique_ptr<JsonDoc> createConf() override;

    const char *getErrorCode() override {return errorCode;}

};

} //namespace v16
} //namespace MicroOcpp
#endif //MO_ENABLE_V16
#endif
