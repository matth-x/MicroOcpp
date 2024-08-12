// matth-x/MicroOcpp
// Copyright Matthias Akstaller 2019 - 2024
// MIT License

#include <MicroOcpp/Operations/InstallCertificate.h>

#if MO_ENABLE_CERT_MGMT

#include <MicroOcpp/Model/Certificates/CertificateService.h>
#include <MicroOcpp/Debug.h>

using MicroOcpp::Ocpp201::InstallCertificate;
using MicroOcpp::MemJsonDoc;

InstallCertificate::InstallCertificate(CertificateService& certService) : AllocOverrider("v201.Operation.", "InstallCertificate"), certService(certService) {

}

void InstallCertificate::processReq(JsonObject payload) {

    if (!payload.containsKey("certificateType") ||
            !payload.containsKey("certificate")) {
        errorCode = "FormationViolation";
        return;
    }

    InstallCertificateType certificateType;

    const char *certificateTypeCstr = payload["certificateType"] | "_Invalid";

    if (!strcmp(certificateTypeCstr, "V2GRootCertificate")) {
        certificateType = InstallCertificateType_V2GRootCertificate;
    } else if (!strcmp(certificateTypeCstr, "MORootCertificate")) {
        certificateType = InstallCertificateType_MORootCertificate;
    } else if (!strcmp(certificateTypeCstr, "CSMSRootCertificate")) {
        certificateType = InstallCertificateType_CSMSRootCertificate;
    } else if (!strcmp(certificateTypeCstr, "ManufacturerRootCertificate")) {
        certificateType = InstallCertificateType_ManufacturerRootCertificate;
    } else {
        errorCode = "FormationViolation";
        return;
    }

    if (!payload["certificate"].is<const char*>()) {
        errorCode = "FormationViolation";
        return;
    }

    const char *certificate = payload["certificate"];

    auto certStore = certService.getCertificateStore();
    if (!certStore) {
        errorCode = "NotSupported";
        return;
    }

    auto status = certStore->installCertificate(certificateType, certificate);

    switch (status) {
        case InstallCertificateStatus_Accepted:
            this->status = "Accepted";
            break;
        case InstallCertificateStatus_Rejected:
            this->status = "Rejected";
            break;
        case InstallCertificateStatus_Failed:
            this->status = "Failed";
            break;
        default:
            MO_DBG_ERR("internal error");
            errorCode = "InternalError";
            return;
    }

    //operation executed successfully
}

std::unique_ptr<MemJsonDoc> InstallCertificate::createConf(){
    auto doc = makeMemJsonDoc(getMemoryTag(), JSON_OBJECT_SIZE(1));
    JsonObject payload = doc->to<JsonObject>();
    payload["status"] = status;
    return doc;
}

#endif //MO_ENABLE_CERT_MGMT
