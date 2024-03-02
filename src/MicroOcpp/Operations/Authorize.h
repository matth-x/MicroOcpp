// matth-x/MicroOcpp
// Copyright Matthias Akstaller 2019 - 2024
// MIT License

#ifndef AUTHORIZE_H
#define AUTHORIZE_H

#include <MicroOcpp/Core/Operation.h>
#include <MicroOcpp/Operations/CiStrings.h>
#include <MicroOcpp/Version.h>

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

#if MO_ENABLE_V201

#include <MicroOcpp/Model/Authorization/IdToken.h>

namespace MicroOcpp {
namespace Ocpp201 {

class Authorize : public Operation {
private:
    Model& model;
    IdToken idToken;
public:
    Authorize(Model& model, const IdToken& idToken);

    const char* getOperationType() override;

    std::unique_ptr<DynamicJsonDocument> createReq() override;

    void processConf(JsonObject payload) override;

    void processReq(JsonObject payload) override;

    std::unique_ptr<DynamicJsonDocument> createConf() override;

};

} //end namespace Ocpp201
} //end namespace MicroOcpp

#endif //MO_ENABLE_V201

#endif
