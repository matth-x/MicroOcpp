// matth-x/MicroOcpp
// Copyright Matthias Akstaller 2019 - 2024
// MIT License

#ifndef MO_INSTALLCERTIFICATE_H
#define MO_INSTALLCERTIFICATE_H

#include <MicroOcpp/Version.h>

#if MO_ENABLE_CERT_MGMT

#include <MicroOcpp/Core/Operation.h>

namespace MicroOcpp {

class CertificateService;

namespace Ocpp201 {

class InstallCertificate : public Operation, public MemoryManaged {
private:
    CertificateService& certService;
    const char *status = nullptr;
    const char *errorCode = nullptr;
public:
    InstallCertificate(CertificateService& certService);

    const char* getOperationType() override {return "InstallCertificate";}

    void processReq(JsonObject payload) override;

    std::unique_ptr<JsonDoc> createConf() override;

    const char *getErrorCode() override {return errorCode;}
};

} //end namespace Ocpp201
} //end namespace MicroOcpp

#endif //MO_ENABLE_CERT_MGMT
#endif
