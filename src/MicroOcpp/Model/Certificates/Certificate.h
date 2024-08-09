// matth-x/MicroOcpp
// Copyright Matthias Akstaller 2019 - 2024
// MIT License

#ifndef MO_CERTIFICATE_H
#define MO_CERTIFICATE_H

#include <MicroOcpp/Version.h>

#if MO_ENABLE_CERT_MGMT

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

#define MO_MAX_CERT_SIZE 5500 //limit of field `certificate` in InstallCertificateRequest, not counting terminating '\0'. See OCPP 2.0.1 part 2 Data Type 1.30.1

/*
 * See OCPP 2.0.1 part 2 Data Type 3.36
 */
typedef enum GetCertificateIdType {
    GetCertificateIdType_V2GRootCertificate,
    GetCertificateIdType_MORootCertificate,
    GetCertificateIdType_CSMSRootCertificate,
    GetCertificateIdType_V2GCertificateChain,
    GetCertificateIdType_ManufacturerRootCertificate
}   GetCertificateIdType;

/*
 * See OCPP 2.0.1 part 2 Data Type 3.40
 */
typedef enum GetInstalledCertificateStatus {
    GetInstalledCertificateStatus_Accepted,
    GetInstalledCertificateStatus_NotFound
}   GetInstalledCertificateStatus;

/*
 * See OCPP 2.0.1 part 2 Data Type 3.45
 */
typedef enum InstallCertificateType {
    InstallCertificateType_V2GRootCertificate,
    InstallCertificateType_MORootCertificate,
    InstallCertificateType_CSMSRootCertificate,
    InstallCertificateType_ManufacturerRootCertificate
}   InstallCertificateType;

/*
 * See OCPP 2.0.1 part 2 Data Type 3.28
 */
typedef enum InstallCertificateStatus {
    InstallCertificateStatus_Accepted,
    InstallCertificateStatus_Rejected,
    InstallCertificateStatus_Failed
}   InstallCertificateStatus;

/*
 * See OCPP 2.0.1 part 2 Data Type 3.28
 */
typedef enum DeleteCertificateStatus {
    DeleteCertificateStatus_Accepted,
    DeleteCertificateStatus_Failed,
    DeleteCertificateStatus_NotFound
}   DeleteCertificateStatus;

/*
 * See OCPP 2.0.1 part 2 Data Type 3.42
 */
typedef enum HashAlgorithmType {
    HashAlgorithmType_SHA256,
    HashAlgorithmType_SHA384,
    HashAlgorithmType_SHA512
}   HashAlgorithmType;

// Convert HashAlgorithmType into string
#define HashAlgorithmLabel(alg) (alg == HashAlgorithmType_SHA256 ? "SHA256" : \
                                 alg == HashAlgorithmType_SHA384 ? "SHA384" : \
                                 alg == HashAlgorithmType_SHA512 ? "SHA512" : "_Undefined")

// Convert HashAlgorithmType into hash size in bytes (e.g. SHA256 -> 32)
#define HashAlgorithmSize(alg) (alg == HashAlgorithmType_SHA256 ? 32 : \
                                alg == HashAlgorithmType_SHA384 ? 48 : \
                                alg == HashAlgorithmType_SHA512 ? 64 : 0)

typedef struct ocpp_cert_hash {
    enum HashAlgorithmType hashAlgorithm;

    unsigned char issuerNameHash [64]; // hash buf can hold 64 bytes (SHA512). Actual hash size is determined by hash algorithm
    unsigned char issuerKeyHash [64];
    unsigned char serialNumber [20];
    size_t serialNumberLen; // length of serial number in bytes
} ocpp_cert_hash;

bool ocpp_cert_equals(const ocpp_cert_hash *h1, const ocpp_cert_hash *h2);

// Max size of hex-encoded cert hash components
#define MO_CERT_HASH_ISSUER_NAME_KEY_SIZE (128 + 1) // hex-encoding needs two characters per byte + terminating null-byte
#define MO_CERT_HASH_SERIAL_NUMBER_SIZE (40 + 1)

/*
 * Print the issuerNameHash of ocpp_cert_hash as hex-encoded string (e.g. "0123AB") into buf. Bufsize MO_CERT_HASH_ISSUER_NAME_KEY_SIZE is always enough
 *
 * Returns the length not counting the terminating 0 on success, -1 on failure
 */
int ocpp_cert_print_issuerNameHash(const ocpp_cert_hash *src, char *buf, size_t size);

/*
 * Print the issuerKeyHash of ocpp_cert_hash as hex-encoded string (e.g. "0123AB") into buf. Bufsize MO_CERT_HASH_ISSUER_NAME_KEY_SIZE is always enough
 *
 * Returns the length not counting the terminating 0 on success, -1 on failure
 */
int ocpp_cert_print_issuerKeyHash(const ocpp_cert_hash *src, char *buf, size_t size);

/*
 * Print the serialNumber of ocpp_cert_hash as hex-encoded string without leading 0s (e.g. "123AB") into buf. Bufsize MO_CERT_HASH_SERIAL_NUMBER_SIZE is always enough
 *
 * Returns the length not counting the terminating 0 on success, -1 on failure
 */
int ocpp_cert_print_serialNumber(const ocpp_cert_hash *src, char *buf, size_t size);

int ocpp_cert_set_issuerNameHash(ocpp_cert_hash *dst, const char *hex_src, HashAlgorithmType hash_algorithm);

int ocpp_cert_set_issuerKeyHash(ocpp_cert_hash *dst, const char *hex_src, HashAlgorithmType hash_algorithm);

int ocpp_cert_set_serialNumber(ocpp_cert_hash *dst, const char *hex_src);

#ifdef __cplusplus
} //extern "C"

#include <MicroOcpp/Core/Memory.h>

namespace MicroOcpp {

using CertificateHash = ocpp_cert_hash;

/*
 * See OCPP 2.0.1 part 2 Data Type 2.5
 */
struct CertificateChainHash : public AllocOverrider {
    GetCertificateIdType certificateType;
    CertificateHash certificateHashData;
    MemVector<CertificateHash> childCertificateHashData;

    CertificateChainHash() : AllocOverrider("v2.0.1.Certificates.CertificateChainHash"), childCertificateHashData(makeMemVector<CertificateHash>(getMemoryTag())) { }
};

/*
 * Interface which allows MicroOcpp to interact with the certificates managed by the local TLS library
 */
class CertificateStore {
public:
    virtual ~CertificateStore() = default;

    virtual GetInstalledCertificateStatus getCertificateIds(const MemVector<GetCertificateIdType>& certificateType, MemVector<CertificateChainHash>& out) = 0;
    virtual DeleteCertificateStatus deleteCertificate(const CertificateHash& hash) = 0;
    virtual InstallCertificateStatus installCertificate(InstallCertificateType certificateType, const char *certificate) = 0;
};

} //namespace MicroOcpp

#endif //__cplusplus
#endif //MO_ENABLE_CERT_MGMT
#endif
