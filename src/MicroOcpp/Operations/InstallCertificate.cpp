#include <MicroOcpp/Operations/InstallCertificate.h>
#include <MicroOcpp/Model/Certificates/CertificateService.h>
#include <MicroOcpp/Debug.h>

using MicroOcpp::Ocpp201::InstallCertificate;

InstallCertificate::InstallCertificate(CertificateService& certService) : certService(certService) {

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
        certificateType = InstallCertificateType::V2GRootCertificate;
    } else if (!strcmp(certificateTypeCstr, "MORootCertificate")) {
        certificateType = InstallCertificateType::MORootCertificate;
    } else if (!strcmp(certificateTypeCstr, "CSMSRootCertificate")) {
        certificateType = InstallCertificateType::CSMSRootCertificate;
    } else if (!strcmp(certificateTypeCstr, "ManufacturerRootCertificate")) {
        certificateType = InstallCertificateType::ManufacturerRootCertificate;
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
        case InstallCertificateStatus::Accepted:
            this->status = "Accepted";
            break;
        case InstallCertificateStatus::Rejected:
            this->status = "Rejected";
            break;
        case InstallCertificateStatus::Failed:
            this->status = "Failed";
            break;
        default:
            MO_DBG_ERR("internal error");
            errorCode = "InternalError";
            return;
    }

    //operation executed successfully
}

std::unique_ptr<DynamicJsonDocument> InstallCertificate::createConf(){
    auto doc = std::unique_ptr<DynamicJsonDocument>(new DynamicJsonDocument(JSON_OBJECT_SIZE(1)));
    JsonObject payload = doc->to<JsonObject>();
    payload["status"] = status;
    return doc;
}
