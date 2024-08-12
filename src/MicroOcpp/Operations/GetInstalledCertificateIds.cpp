// matth-x/MicroOcpp
// Copyright Matthias Akstaller 2019 - 2024
// MIT License

#include <MicroOcpp/Operations/GetInstalledCertificateIds.h>

#if MO_ENABLE_CERT_MGMT

#include <MicroOcpp/Model/Certificates/Certificate.h>
#include <MicroOcpp/Model/Certificates/CertificateService.h>
#include <MicroOcpp/Debug.h>

using MicroOcpp::Ocpp201::GetInstalledCertificateIds;
using MicroOcpp::MemJsonDoc;

GetInstalledCertificateIds::GetInstalledCertificateIds(CertificateService& certService) : AllocOverrider("v201.Operation.", "GetInstalledCertificateIds"), certService(certService), certificateHashDataChain(makeMemVector<CertificateChainHash>(getMemoryTag())) {

}

void GetInstalledCertificateIds::processReq(JsonObject payload) {

    if (!payload.containsKey("certificateType")) {
        errorCode = "FormationViolation";
        return;
    }

    MemVector<GetCertificateIdType> certificateType = makeMemVector<GetCertificateIdType>(getMemoryTag());
    for (const char *certificateTypeCstr : payload["certificateType"].as<JsonArray>()) {
        if (!strcmp(certificateTypeCstr, "V2GRootCertificate")) {
            certificateType.push_back(GetCertificateIdType_V2GRootCertificate);
        } else if (!strcmp(certificateTypeCstr, "MORootCertificate")) {
            certificateType.push_back(GetCertificateIdType_MORootCertificate);
        } else if (!strcmp(certificateTypeCstr, "CSMSRootCertificate")) {
            certificateType.push_back(GetCertificateIdType_CSMSRootCertificate);
        } else if (!strcmp(certificateTypeCstr, "V2GCertificateChain")) {
            certificateType.push_back(GetCertificateIdType_V2GCertificateChain);
        } else if (!strcmp(certificateTypeCstr, "ManufacturerRootCertificate")) {
            certificateType.push_back(GetCertificateIdType_ManufacturerRootCertificate);
        } else {
            errorCode = "FormationViolation";
            return;
        }
    }

    auto certStore = certService.getCertificateStore();
    if (!certStore) {
        errorCode = "NotSupported";
        return;
    }

    auto status = certStore->getCertificateIds(certificateType, certificateHashDataChain);

    switch (status) {
        case GetInstalledCertificateStatus_Accepted:
            this->status = "Accepted";
            break;
        case GetInstalledCertificateStatus_NotFound:
            this->status = "NotFound";
            break;
        default:
            MO_DBG_ERR("internal error");
            errorCode = "InternalError";
            return;
    }

    //operation executed successfully
}

std::unique_ptr<MemJsonDoc> GetInstalledCertificateIds::createConf() {

    size_t capacity =
            JSON_OBJECT_SIZE(2) + //payload root
            JSON_ARRAY_SIZE(certificateHashDataChain.size()); //array for field certificateHashDataChain
    for (auto& cch : certificateHashDataChain) {
        capacity +=
                JSON_OBJECT_SIZE(2) + //certificateHashDataChain root
                JSON_OBJECT_SIZE(4) +  //certificateHashData
                (2 * HashAlgorithmSize(cch.certificateHashData.hashAlgorithm) + //issuerNameHash and issuerKeyHash
                    cch.certificateHashData.serialNumberLen)
                    * 2 + 3; //issuerNameHash, issuerKeyHash and serialNumber as hex-endoded cstring
    }

    auto doc = makeMemJsonDoc(getMemoryTag(), capacity);
    JsonObject payload = doc->to<JsonObject>();
    payload["status"] = status;

    for (auto& chainElem : certificateHashDataChain) {
        JsonObject certHashJson = payload["certificateHashDataChain"].createNestedObject();

        const char *certificateTypeCstr = "";
        switch (chainElem.certificateType) {
            case GetCertificateIdType_V2GRootCertificate:
                certificateTypeCstr = "V2GRootCertificate";
                break;
            case GetCertificateIdType_MORootCertificate:
                certificateTypeCstr = "MORootCertificate";
                break;
            case GetCertificateIdType_CSMSRootCertificate:
                certificateTypeCstr = "CSMSRootCertificate";
                break;
            case GetCertificateIdType_V2GCertificateChain:
                certificateTypeCstr = "V2GCertificateChain";
                break;
            case GetCertificateIdType_ManufacturerRootCertificate:
                certificateTypeCstr = "ManufacturerRootCertificate";
                break;
        }

        certHashJson["certificateType"] = (const char*) certificateTypeCstr; //use JSON zero-copy mode
        certHashJson["certificateHashData"]["hashAlgorithm"] = HashAlgorithmLabel(chainElem.certificateHashData.hashAlgorithm);

        char buf [MO_CERT_HASH_ISSUER_NAME_KEY_SIZE];

        ocpp_cert_print_issuerNameHash(&chainElem.certificateHashData, buf, sizeof(buf));
        certHashJson["certificateHashData"]["issuerNameHash"] = buf;

        ocpp_cert_print_issuerKeyHash(&chainElem.certificateHashData, buf, sizeof(buf));
        certHashJson["certificateHashData"]["issuerKeyHash"] = buf;

        ocpp_cert_print_serialNumber(&chainElem.certificateHashData, buf, sizeof(buf));
        certHashJson["certificateHashData"]["serialNumber"] = buf;
        
        if (!chainElem.childCertificateHashData.empty()) {
            MO_DBG_ERR("only sole root certs supported");
        }
    }

    return doc;
}

#endif //MO_ENABLE_CERT_MGMT
