// matth-x/MicroOcpp
// Copyright Matthias Akstaller 2019 - 2024
// MIT License

#ifndef MO_SENDLOCALLIST_H
#define MO_SENDLOCALLIST_H

#include <MicroOcpp/Core/Operation.h>
#include <MicroOcpp/Version.h>

#if MO_ENABLE_V16 && MO_ENABLE_LOCAL_AUTH

namespace MicroOcpp {
namespace Ocpp16 {

class AuthorizationService;

class SendLocalList : public Operation, public MemoryManaged {
private:
    AuthorizationService& authService;
    const char *errorCode = nullptr;
    bool updateFailure = true;
    bool versionMismatch = false;
public:
    SendLocalList(AuthorizationService& authService);

    ~SendLocalList();

    const char* getOperationType() override;

    void processReq(JsonObject payload) override;

    std::unique_ptr<JsonDoc> createConf() override;

    const char *getErrorCode() override {return errorCode;}
};

} //namespace Ocpp16
} //namespace MicroOcpp
#endif //MO_ENABLE_V16 && MO_ENABLE_LOCAL_AUTH
#endif
