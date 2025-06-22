// matth-x/MicroOcpp
// Copyright Matthias Akstaller 2019 - 2024
// MIT License

#ifndef MO_AUTHORIZE_H
#define MO_AUTHORIZE_H

#include <MicroOcpp/Core/Operation.h>
#include <MicroOcpp/Model/Authorization/IdToken.h>
#include <MicroOcpp/Version.h>

#if MO_ENABLE_V16

namespace MicroOcpp {
namespace Ocpp16 {

class Model;

class Authorize : public Operation, public MemoryManaged {
private:
    Model& model;
    char idTag [MO_IDTAG_LEN_MAX + 1] = {'\0'};
public:
    Authorize(Model& model, const char *idTag);

    const char* getOperationType() override;

    std::unique_ptr<JsonDoc> createReq() override;

    void processConf(JsonObject payload) override;

#if MO_ENABLE_MOCK_SERVER
    static int writeMockConf(const char *operationType, char *buf, size_t size, void *userStatus, void *userData);
#endif

};

} //namespace Ocpp16
} //namespace MicroOcpp

#endif //MO_ENABLE_V16

#if MO_ENABLE_V201

#include <MicroOcpp/Model/Authorization/IdToken.h>

namespace MicroOcpp {
namespace Ocpp201 {

class Model;

class Authorize : public Operation, public MemoryManaged {
private:
    Model& model;
    IdToken idToken;
public:
    Authorize(Model& model, const IdToken& idToken);

    const char* getOperationType() override;

    std::unique_ptr<JsonDoc> createReq() override;

    void processConf(JsonObject payload) override;

#if MO_ENABLE_MOCK_SERVER
    static int writeMockConf(const char *operationType, char *buf, size_t size, void *userStatus, void *userData);
#endif

};

} //namespace Ocpp201
} //namespace MicroOcpp

#endif //MO_ENABLE_V201

#endif
