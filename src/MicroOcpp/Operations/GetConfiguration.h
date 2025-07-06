// matth-x/MicroOcpp
// Copyright Matthias Akstaller 2019 - 2024
// MIT License

#ifndef MO_GETCONFIGURATION_H
#define MO_GETCONFIGURATION_H

#include <MicroOcpp/Core/Operation.h>
#include <MicroOcpp/Core/Memory.h>
#include <MicroOcpp/Version.h>

#if MO_ENABLE_V16

namespace MicroOcpp {
namespace v16 {

class Configuration;
class ConfigurationService;

class GetConfiguration : public Operation, public MemoryManaged {
private:
    ConfigurationService& configService;

    Vector<Configuration*> configurations;
    Vector<char*> unknownKeys;

    const char *errorCode {nullptr};
    const char *errorDescription = "";
public:
    GetConfiguration(ConfigurationService& configService);
    ~GetConfiguration();

    const char* getOperationType() override;

    void processReq(JsonObject payload) override;

    std::unique_ptr<JsonDoc> createConf() override;

    const char *getErrorCode() override {return errorCode;}
    const char *getErrorDescription() override {return errorDescription;}

};

} //namespace v16
} //namespace MicroOcpp
#endif //MO_ENABLE_V16
#endif
