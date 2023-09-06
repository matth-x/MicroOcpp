// matth-x/MicroOcpp
// Copyright Matthias Akstaller 2019 - 2023
// MIT License

#ifndef AUTHORIZE_H
#define AUTHORIZE_H

#include <MicroOcpp/Core/Operation.h>
#include <MicroOcpp/Operations/CiStrings.h>

namespace MicroOcpp {

class Model;

namespace Ocpp16 {

class Authorize : public Operation {
private:
    Model& model;
    char idTag [IDTAG_LEN_MAX + 1] = {'\0'};
public:
    Authorize(Model& model, const char *idTag);

    const char* getOperationType() override;

    std::unique_ptr<DynamicJsonDocument> createReq() override;

    void processConf(JsonObject payload) override;

    void processReq(JsonObject payload) override;

    std::unique_ptr<DynamicJsonDocument> createConf() override;

};

} //end namespace Ocpp16
} //end namespace MicroOcpp

#endif
