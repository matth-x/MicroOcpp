// matth-x/MicroOcpp
// Copyright Matthias Akstaller 2019 - 2024
// MIT License

#ifndef MO_CERTIFICATE_H
#define MO_CERTIFICATE_H

#include <vector>
#include <stdint.h>

namespace MicroOcpp {

#define MO_MAX_CERT_SIZE 5500 //limit of field `certificate` in InstallCertificateRequest, not counting terminating '\0'. See OCPP 2.0.1 part 2 Data Type 1.30.1

/*
 * See OCPP 2.0.1 part 2 Data Type 3.36
 */
enum class GetCertificateIdType : uint8_t {
    V2GRootCertificate,
    MORootCertificate,
    CSMSRootCertificate,
    V2GCertificateChain,
    ManufacturerRootCertificate
};

/*
 * See OCPP 2.0.1 part 2 Data Type 3.40
 */
enum class GetInstalledCertificateStatus : uint8_t {
    Accepted,
    NotFound
};

/*
 * See OCPP 2.0.1 part 2 Data Type 3.45
 */
enum class InstallCertificateType : uint8_t {
    V2GRootCertificate,
    MORootCertificate,
    CSMSRootCertificate,
    ManufacturerRootCertificate
};

/*
 * See OCPP 2.0.1 part 2 Data Type 3.28
 */
enum class InstallCertificateStatus : uint8_t {
    Accepted,
    Rejected,
    Failed
};

/*
 * See OCPP 2.0.1 part 2 Data Type 3.28
 */
enum class DeleteCertificateStatus : uint8_t {
    Accepted,
    Failed,
    NotFound
};

/*
 * See OCPP 2.0.1 part 2 Data Type 3.42
 */
enum class HashAlgorithmEnumType : uint8_t {
    SHA256,
    SHA384,
    SHA512
};

/*
 * See OCPP 2.0.1 part 2 Data Type 2.6
 */
struct CertificateHash {
    HashAlgorithmEnumType hashAlgorithm;
    char issuerNameHash [128 + 1];
    char issuerKeyHash [128 + 1];
    char serialNumber [40 + 1];

    const char *getHashAlgorithmCStr();
    const char *getIssuerNameHash();
    const char *getIssuerKeyHash();
    const char *getSerialNumber();

    bool equals(const CertificateHash& other);
};

/*
 * See OCPP 2.0.1 part 2 Data Type 2.5
 */
struct CertificateChainHash {
    GetCertificateIdType certificateType;
    CertificateHash certificateHashData;
    std::vector<CertificateHash> childCertificateHashData;
};

/*
 * Interface which allows MicroOcpp to interact with the certificates managed by the local TLS library
 */
class CertificateStore {
public:
    virtual GetInstalledCertificateStatus getCertificateIds(GetCertificateIdType certificateType, std::vector<CertificateChainHash>& out) = 0;
    virtual DeleteCertificateStatus deleteCertificate(const CertificateHash& hash) = 0;
    virtual InstallCertificateStatus installCertificate(InstallCertificateType certificateType, const char *certificate) = 0;
};

} //namespace MicroOcpp

#endif
