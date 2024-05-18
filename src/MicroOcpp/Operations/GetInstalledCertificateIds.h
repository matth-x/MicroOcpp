// matth-x/MicroOcpp
// Copyright Matthias Akstaller 2019 - 2024
// MIT License

#ifndef MO_GETINSTALLEDCERTIFICATEIDS_H
#define MO_GETINSTALLEDCERTIFICATEIDS_H

#include <MicroOcpp/Version.h>

#if MO_ENABLE_CERT_MGMT

#include <MicroOcpp/Core/Operation.h>
#include <MicroOcpp/Model/Certificates/Certificate.h>

namespace MicroOcpp {

class CertificateService;

namespace Ocpp201 {

class GetInstalledCertificateIds : public Operation {
private:
    CertificateService& certService;
    std::vector<CertificateChainHash> certificateHashDataChain;
    const char *status = nullptr;
    const char *errorCode = nullptr;
public:
    GetInstalledCertificateIds(CertificateService& certService);

    const char* getOperationType() override {return "GetInstalledCertificateIds";}

    void processReq(JsonObject payload) override;

    std::unique_ptr<DynamicJsonDocument> createConf() override;

    const char *getErrorCode() override {return errorCode;}
};

} //end namespace Ocpp201
} //end namespace MicroOcpp

#endif //MO_ENABLE_CERT_MGMT
#endif
