// matth-x/MicroOcpp
// Copyright Matthias Akstaller 2019 - 2024
// MIT License

#ifndef MO_DELETECERTIFICATE_H
#define MO_DELETECERTIFICATE_H

#include <MicroOcpp/Core/Operation.h>
#include <MicroOcpp/Version.h>

#if (MO_ENABLE_V16 || MO_ENABLE_V201) && MO_ENABLE_CERT_MGMT

namespace MicroOcpp {

class CertificateService;

class DeleteCertificate : public Operation, public MemoryManaged {
private:
    CertificateService& certService;
    const char *status = nullptr;
    const char *errorCode = nullptr;
public:
    DeleteCertificate(CertificateService& certService);

    const char* getOperationType() override {return "DeleteCertificate";}

    void processReq(JsonObject payload) override;

    std::unique_ptr<JsonDoc> createConf() override;

    const char *getErrorCode() override {return errorCode;}
};

} //namespace MicroOcpp

#endif //(MO_ENABLE_V16 || MO_ENABLE_V201) && MO_ENABLE_CERT_MGMT
#endif
