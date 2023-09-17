// matth-x/MicroOcpp
// Copyright Matthias Akstaller 2019 - 2023
// MIT License

#ifndef GETCONFIGURATION_H
#define GETCONFIGURATION_H

#include <MicroOcpp/Core/Operation.h>

#include <vector>

namespace MicroOcpp {
namespace Ocpp16 {

class GetConfiguration : public Operation {
private:
    std::vector<std::string> keys;

    const char *errorCode {nullptr};
    const char *errorDescription = "";
public:
    GetConfiguration();

    const char* getOperationType() override;

    void processReq(JsonObject payload) override;

    std::unique_ptr<DynamicJsonDocument> createConf() override;

    const char *getErrorCode() override {return errorCode;}
    const char *getErrorDescription() override {return errorDescription;}

};

} //end namespace Ocpp16
} //end namespace MicroOcpp
#endif
