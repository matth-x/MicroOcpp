// matth-x/MicroOcpp
// Copyright Matthias Akstaller 2019 - 2024
// MIT License

#ifndef MO_CERTIFICATE_C_H
#define MO_CERTIFICATE_C_H

#include <MicroOcpp/Version.h>

#if MO_ENABLE_CERT_MGMT

#include <stddef.h>

#include <MicroOcpp/Model/Certificates/Certificate.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct ocpp_cert_chain_hash {
    void *user_data; //set this at your choice. MO passes it back to the functions below

    enum GetCertificateIdType certType;
    ocpp_cert_hash certHashData;
    //ocpp_cert_hash *childCertificateHashData; 

    struct ocpp_cert_chain_hash *next; //link to next list element if result of getCertificateIds

    void (*invalidate)(void *user_data); //free resources here. Guaranteed to be called
} ocpp_cert_chain_hash;

typedef struct ocpp_cert_store {
    void *user_data; //set this at your choice. MO passes it back to the functions below

    enum GetInstalledCertificateStatus (*getCertificateIds)(void *user_data, const enum GetCertificateIdType certType [], size_t certTypeLen, ocpp_cert_chain_hash **out);
    enum DeleteCertificateStatus (*deleteCertificate)(void *user_data, const ocpp_cert_hash *hash);
    enum InstallCertificateStatus (*installCertificate)(void *user_data, enum InstallCertificateType certType, const char *cert);
} ocpp_cert_store;

#ifdef __cplusplus
} //extern "C"

#include <memory>

namespace MicroOcpp {

std::unique_ptr<CertificateStore> makeCertificateStoreCwrapper(ocpp_cert_store *certstore);

} //namespace MicroOcpp

#endif //__cplusplus

#endif //MO_ENABLE_CERT_MGMT
#endif
