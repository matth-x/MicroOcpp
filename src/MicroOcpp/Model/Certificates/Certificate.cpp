// matth-x/MicroOcpp
// Copyright Matthias Akstaller 2019 - 2024
// MIT License

#include <MicroOcpp/Model/Certificates/Certificate.h>

#include <string.h>

#include <MicroOcpp/Debug.h>

using namespace MicroOcpp;

const char *CertificateHash::getHashAlgorithmCStr() {
    switch(hashAlgorithm) {
        case HashAlgorithmEnumType::SHA256:
            return "SHA256";
        case HashAlgorithmEnumType::SHA384:
            return "SHA384";
        case HashAlgorithmEnumType::SHA512:
            return "SHA512";
    }

    MO_DBG_ERR("internal error");
    return "";
}

const char *CertificateHash::getIssuerNameHash() {
    return issuerNameHash;
}

const char *CertificateHash::getIssuerKeyHash() {
    return issuerKeyHash;
}

const char *CertificateHash::getSerialNumber() {
    return serialNumber;
}

bool CertificateHash::equals(const CertificateHash& other) {
    return hashAlgorithm == other.hashAlgorithm &&
            !strncmp(serialNumber, other.serialNumber, sizeof(serialNumber)) &&
            !strncmp(issuerNameHash, other.issuerNameHash, sizeof(issuerNameHash)) &&
            !strncmp(issuerKeyHash, other.issuerKeyHash, sizeof(issuerKeyHash));
}
