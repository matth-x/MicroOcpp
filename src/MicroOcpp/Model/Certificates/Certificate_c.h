// matth-x/MicroOcpp
// Copyright Matthias Akstaller 2019 - 2024
// MIT License

#ifndef MO_CERTIFICATE_C_H
#define MO_CERTIFICATE_C_H

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

enum GetCertificateIdType_c {
    ENUM_GCI_V2GRootCertificate,
    ENUM_GCI_MORootCertificate,
    ENUM_GCI_CSMSRootCertificate,
    ENUM_GCI_V2GCertificateChain,
    ENUM_GCI_ManufacturerRootCertificate
};

/*
 * See OCPP 2.0.1 part 2 Data Type 3.40
 */
enum GetInstalledCertificateStatus_c {
    ENUM_GICS_Accepted,
    ENUM_GICS_NotFound
};

/*
 * See OCPP 2.0.1 part 2 Data Type 3.45
 */
enum InstallCertificateType_c {
    ENUM_IC_V2GRootCertificate,
    ENUM_IC_MORootCertificate,
    ENUM_IC_CSMSRootCertificate,
    ENUM_IC_ManufacturerRootCertificate
};

/*
 * See OCPP 2.0.1 part 2 Data Type 3.28
 */
enum InstallCertificateStatus_c {
    ENUM_ICS_Accepted,
    ENUM_ICS_Rejected,
    ENUM_ICS_Failed
};

/*
 * See OCPP 2.0.1 part 2 Data Type 3.28
 */
enum DeleteCertificateStatus_c {
    ENUM_DCS_Accepted,
    ENUM_DCS_Failed,
    ENUM_DCS_NotFound
};

/*
 * See OCPP 2.0.1 part 2 Data Type 3.42
 */
enum HashAlgorithmEnumType_c {
    ENUM_HA_SHA256,
    ENUM_HA_SHA384,
    ENUM_HA_SHA512
};

#define MO_CERT_HASH_ISSUER_NAME_SIZE (128 + 1)
#define MO_CERT_HASH_ISSUER_KEY_SIZE (128 + 1)
#define MO_CERT_HASH_SERIAL_NUMBER_SIZE (40 + 1)

typedef struct ocpp_certificate_hash {
    enum HashAlgorithmEnumType_c hashAlgorithm;
    char issuerNameHash [MO_CERT_HASH_ISSUER_NAME_SIZE];
    char issuerKeyHash [MO_CERT_HASH_ISSUER_KEY_SIZE];
    char serialNumber [MO_CERT_HASH_SERIAL_NUMBER_SIZE];

    //ocpp_certificate_hash *next; //link to next list element if part of ocpp_certificate_chain_hash
} ocpp_certificate_hash;

typedef struct ocpp_certificate_chain_hash {
    void *user_data; //set this at your choice. MO passes it back to the functions below

    enum GetCertificateIdType_c certificateType;
    ocpp_certificate_hash certificateHashData;
    //ocpp_certificate_hash *childCertificateHashData; 

    struct ocpp_certificate_chain_hash *next; //link to next list element if result of getCertificateIds

    void (*invalidate)(void *user_data); //free resources here. Guaranteed to be called
} ocpp_certificate_chain_hash;

typedef struct ocpp_certificate_store {
    void *user_data; //set this at your choice. MO passes it back to the functions below

    enum GetInstalledCertificateStatus_c (*getCertificateIds)(void *user_data, enum GetCertificateIdType_c certificateType [], size_t certificateTypeLen, ocpp_certificate_chain_hash **out);
    enum DeleteCertificateStatus_c (*deleteCertificate)(void *user_data, const ocpp_certificate_hash *hash);
    enum InstallCertificateStatus_c (*installCertificate)(void *user_data, enum InstallCertificateType_c certificateType, const char *certificate);
} ocpp_certificate_store;

#ifdef __cplusplus
} //extern "C"

#include <memory>

#include <MicroOcpp/Model/Certificates/Certificate.h>

namespace MicroOcpp {

std::unique_ptr<CertificateStore> makeCertificateStoreCwrapper(ocpp_certificate_store *certstore);

} //namespace MicroOcpp

#endif //defined __cplusplus

#endif
