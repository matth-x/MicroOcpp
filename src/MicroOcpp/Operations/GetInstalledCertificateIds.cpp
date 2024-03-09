// matth-x/MicroOcpp
// Copyright Matthias Akstaller 2019 - 2024
// MIT License

#include <MicroOcpp/Operations/GetInstalledCertificateIds.h>
#include <MicroOcpp/Model/Certificates/Certificate.h>
#include <MicroOcpp/Model/Certificates/CertificateService.h>
#include <MicroOcpp/Debug.h>

using MicroOcpp::Ocpp201::GetInstalledCertificateIds;

GetInstalledCertificateIds::GetInstalledCertificateIds(CertificateService& certService) : certService(certService) {

}

void GetInstalledCertificateIds::processReq(JsonObject payload) {

    if (!payload.containsKey("certificateType")) {
        errorCode = "FormationViolation";
        return;
    }

    const char *certificateTypeCstr = payload["certificateType"] | "_Invalid";
    GetCertificateIdType certificateType;
    if (!strcmp(certificateTypeCstr, "V2GRootCertificate")) {
        certificateType = GetCertificateIdType::V2GRootCertificate;
    } else if (!strcmp(certificateTypeCstr, "MORootCertificate")) {
        certificateType = GetCertificateIdType::MORootCertificate;
    } else if (!strcmp(certificateTypeCstr, "CSMSRootCertificate")) {
        certificateType = GetCertificateIdType::CSMSRootCertificate;
    } else if (!strcmp(certificateTypeCstr, "V2GCertificateChain")) {
        certificateType = GetCertificateIdType::V2GCertificateChain;
    } else if (!strcmp(certificateTypeCstr, "ManufacturerRootCertificate")) {
        certificateType = GetCertificateIdType::ManufacturerRootCertificate;
    } else {
        errorCode = "FormationViolation";
        return;
    }

    auto certStore = certService.getCertificateStore();
    if (!certStore) {
        errorCode = "NotSupported";
        return;
    }

    auto status = certStore->getCertificateIds(certificateType, certificateHashDataChain);

    switch (status) {
        case GetInstalledCertificateStatus::Accepted:
            this->status = "Accepted";
            break;
        case GetInstalledCertificateStatus::NotFound:
            this->status = "NotFound";
            break;
        default:
            MO_DBG_ERR("internal error");
            errorCode = "InternalError";
            return;
    }

    //operation executed successfully
}

std::unique_ptr<DynamicJsonDocument> GetInstalledCertificateIds::createConf(){
    auto doc = std::unique_ptr<DynamicJsonDocument>(new DynamicJsonDocument(
            JSON_OBJECT_SIZE(2) + //payload root
            JSON_ARRAY_SIZE(certificateHashDataChain.size()) + //array for field certificateHashDataChain
            certificateHashDataChain.size() * ( 
                JSON_OBJECT_SIZE(2) + //certificateHashDataChain root
                JSON_OBJECT_SIZE(4))  //certificateHashData
            ));
    JsonObject payload = doc->to<JsonObject>();
    payload["status"] = status;

    for (auto& chainElem : certificateHashDataChain) {
        JsonObject certHashJson = payload["certificateHashDataChain"].createNestedObject();

        const char *certificateTypeCstr = "";
        switch (chainElem.certificateType) {
            case GetCertificateIdType::V2GRootCertificate:
                certificateTypeCstr = "V2GRootCertificate";
                break;
            case GetCertificateIdType::MORootCertificate:
                certificateTypeCstr = "MORootCertificate";
                break;
            case GetCertificateIdType::CSMSRootCertificate:
                certificateTypeCstr = "CSMSRootCertificate";
                break;
            case GetCertificateIdType::V2GCertificateChain:
                certificateTypeCstr = "V2GCertificateChain";
                break;
            case GetCertificateIdType::ManufacturerRootCertificate:
                certificateTypeCstr = "ManufacturerRootCertificate";
                break;
        }

        certHashJson["certificateType"] = (const char*) certificateTypeCstr; //use JSON zero-copy mode
        certHashJson["certificateHashData"]["hashAlgorithm"] = chainElem.certificateHashData.getHashAlgorithmCStr();
        certHashJson["certificateHashData"]["issuerNameHash"] = chainElem.certificateHashData.getIssuerNameHash();
        certHashJson["certificateHashData"]["issuerKeyHash"] = chainElem.certificateHashData.getIssuerKeyHash();
        certHashJson["certificateHashData"]["serialNumber"] = chainElem.certificateHashData.getSerialNumber();
        
        if (!chainElem.childCertificateHashData.empty()) {
            MO_DBG_ERR("only sole root certs supported");
        }
    }

    return doc;
}
