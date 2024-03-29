// matth-x/MicroOcpp
// Copyright Matthias Akstaller 2019 - 2024
// MIT License

#include <MicroOcpp/Operations/DeleteCertificate.h>
#include <MicroOcpp/Model/Certificates/Certificate.h>
#include <MicroOcpp/Model/Certificates/CertificateService.h>
#include <MicroOcpp/Debug.h>

using MicroOcpp::Ocpp201::DeleteCertificate;

DeleteCertificate::DeleteCertificate(CertificateService& certService) : certService(certService) {

}

void DeleteCertificate::processReq(JsonObject payload) {

    JsonObject certIdJson = payload["certificateHashData"];

    if (!certIdJson.containsKey("hashAlgorithm") ||
            !certIdJson.containsKey("issuerNameHash") ||
            !certIdJson.containsKey("issuerKeyHash") ||
            !certIdJson.containsKey("serialNumber")) {
        errorCode = "FormationViolation";
        return;
    }

    const char *hashAlgorithm = certIdJson["hashAlgorithm"] | "_Invalid";

    if (!certIdJson["issuerNameHash"].is<const char*>() ||
            !certIdJson["issuerKeyHash"].is<const char*>() ||
            !certIdJson["serialNumber"].is<const char*>()) {
        errorCode = "FormationViolation";
        return;
    }

    CertificateHash cert;

    if (!strcmp(hashAlgorithm, "SHA256")) {
        cert.hashAlgorithm = HashAlgorithmEnumType::SHA256;
    } else if (!strcmp(hashAlgorithm, "SHA384")) {
        cert.hashAlgorithm = HashAlgorithmEnumType::SHA384;
    } else if (!strcmp(hashAlgorithm, "SHA512")) {
        cert.hashAlgorithm = HashAlgorithmEnumType::SHA512;
    } else {
        errorCode = "FormationViolation";
        return;
    }

    auto retIN = snprintf(cert.issuerNameHash, sizeof(cert.issuerNameHash), "%s", certIdJson["issuerNameHash"] | "_Invalid");
    auto retIK = snprintf(cert.issuerKeyHash, sizeof(cert.issuerKeyHash), "%s", certIdJson["issuerKeyHash"] | "_Invalid");
    auto retSN = snprintf(cert.serialNumber, sizeof(cert.serialNumber), "%s", certIdJson["serialNumber"] | "_Invalid");
    if (retIN < 0 || retIK < 0 || retSN < 0) {
        MO_DBG_ERR("could not parse CertId: %i %i %i", retIN, retIK, retSN);
        errorCode = "InternalError";
        return;
    }
    if ((size_t)retIN >= sizeof(cert.issuerNameHash) ||
            (size_t)retIK >= sizeof(cert.issuerKeyHash) ||
            (size_t)retSN >= sizeof(cert.serialNumber)) {
        errorCode = "FormationViolation";
        return;
    }

    auto certStore = certService.getCertificateStore();
    if (!certStore) {
        errorCode = "NotSupported";
        return;
    }

    auto status = certStore->deleteCertificate(cert);

    switch (status) {
        case DeleteCertificateStatus::Accepted:
            this->status = "Accepted";
            break;
        case DeleteCertificateStatus::Failed:
            this->status = "Failed";
            break;
        case DeleteCertificateStatus::NotFound:
            this->status = "NotFound";
            break;
        default:
            MO_DBG_ERR("internal error");
            errorCode = "InternalError";
            return;
    }

    //operation executed successfully
}

std::unique_ptr<DynamicJsonDocument> DeleteCertificate::createConf(){
    auto doc = std::unique_ptr<DynamicJsonDocument>(new DynamicJsonDocument(JSON_OBJECT_SIZE(1)));
    JsonObject payload = doc->to<JsonObject>();
    payload["status"] = status;
    return doc;
}
