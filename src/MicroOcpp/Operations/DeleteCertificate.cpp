// matth-x/MicroOcpp
// Copyright Matthias Akstaller 2019 - 2024
// MIT License

#include <MicroOcpp/Operations/DeleteCertificate.h>

#if MO_ENABLE_CERT_MGMT

#include <MicroOcpp/Model/Certificates/Certificate.h>
#include <MicroOcpp/Model/Certificates/CertificateService.h>
#include <MicroOcpp/Debug.h>

using MicroOcpp::Ocpp201::DeleteCertificate;
using MicroOcpp::MemJsonDoc;

DeleteCertificate::DeleteCertificate(CertificateService& certService) : AllocOverrider("v201.Operation.", "DeleteCertificate"), certService(certService) {

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
        cert.hashAlgorithm = HashAlgorithmType_SHA256;
    } else if (!strcmp(hashAlgorithm, "SHA384")) {
        cert.hashAlgorithm = HashAlgorithmType_SHA384;
    } else if (!strcmp(hashAlgorithm, "SHA512")) {
        cert.hashAlgorithm = HashAlgorithmType_SHA512;
    } else {
        errorCode = "FormationViolation";
        return;
    }

    auto retIN = ocpp_cert_set_issuerNameHash(&cert, certIdJson["issuerNameHash"] | "_Invalid", cert.hashAlgorithm);
    auto retIK = ocpp_cert_set_issuerKeyHash(&cert, certIdJson["issuerKeyHash"] | "_Invalid", cert.hashAlgorithm);
    auto retSN = ocpp_cert_set_serialNumber(&cert, certIdJson["serialNumber"] | "_Invalid");
    if (retIN < 0 || retIK < 0 || retSN < 0) {
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
        case DeleteCertificateStatus_Accepted:
            this->status = "Accepted";
            break;
        case DeleteCertificateStatus_Failed:
            this->status = "Failed";
            break;
        case DeleteCertificateStatus_NotFound:
            this->status = "NotFound";
            break;
        default:
            MO_DBG_ERR("internal error");
            errorCode = "InternalError";
            return;
    }

    //operation executed successfully
}

std::unique_ptr<MemJsonDoc> DeleteCertificate::createConf(){
    auto doc = makeMemJsonDoc(getMemoryTag(), JSON_OBJECT_SIZE(1));
    JsonObject payload = doc->to<JsonObject>();
    payload["status"] = status;
    return doc;
}

#endif //MO_ENABLE_CERT_MGMT
