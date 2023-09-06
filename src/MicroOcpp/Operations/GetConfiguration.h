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
public:
    GetConfiguration();

    const char* getOperationType() override;

    void processReq(JsonObject payload) override;

    std::unique_ptr<DynamicJsonDocument> createConf() override;

};

} //end namespace Ocpp16
} //end namespace MicroOcpp
#endif
