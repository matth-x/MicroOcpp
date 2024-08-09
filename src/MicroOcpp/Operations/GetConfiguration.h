// matth-x/MicroOcpp
// Copyright Matthias Akstaller 2019 - 2024
// MIT License

#ifndef MO_GETCONFIGURATION_H
#define MO_GETCONFIGURATION_H

#include <MicroOcpp/Core/Operation.h>

#include <vector>

namespace MicroOcpp {
namespace Ocpp16 {

class GetConfiguration : public Operation, public AllocOverrider {
private:
    MemVector<MemString> keys;

    const char *errorCode {nullptr};
    const char *errorDescription = "";
public:
    GetConfiguration();

    const char* getOperationType() override;

    void processReq(JsonObject payload) override;

    std::unique_ptr<MemJsonDoc> createConf() override;

    const char *getErrorCode() override {return errorCode;}
    const char *getErrorDescription() override {return errorDescription;}

};

} //end namespace Ocpp16
} //end namespace MicroOcpp
#endif
