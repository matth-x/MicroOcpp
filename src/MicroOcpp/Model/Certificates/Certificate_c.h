// matth-x/MicroOcpp
// Copyright Matthias Akstaller 2019 - 2024
// MIT License

#ifndef MO_CERTIFICATE_C_H
#define MO_CERTIFICATE_C_H

#include <MicroOcpp/Version.h>

#if (MO_ENABLE_V16 || MO_ENABLE_V201) && MO_ENABLE_CERT_MGMT

#include <stddef.h>

#include <MicroOcpp/Model/Certificates/Certificate.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct mo_cert_chain_hash {
    void *user_data; //set this at your choice. MO passes it back to the functions below

    enum GetCertificateIdType certType;
    mo_cert_hash certHashData;
    //mo_cert_hash *childCertificateHashData; 

    struct mo_cert_chain_hash *next; //link to next list element if result of getCertificateIds

    void (*invalidate)(void *user_data); //free resources here. Guaranteed to be called
} mo_cert_chain_hash;

typedef struct mo_cert_store {
    void *user_data; //set this at your choice. MO passes it back to the functions below

    enum GetInstalledCertificateStatus (*getCertificateIds)(void *user_data, const enum GetCertificateIdType certType [], size_t certTypeLen, mo_cert_chain_hash **out);
    enum DeleteCertificateStatus (*deleteCertificate)(void *user_data, const mo_cert_hash *hash);
    enum InstallCertificateStatus (*installCertificate)(void *user_data, enum InstallCertificateType certType, const char *cert);
} mo_cert_store;

#ifdef __cplusplus
} //extern "C"

#include <memory>

namespace MicroOcpp {

std::unique_ptr<CertificateStore> makeCertificateStoreCwrapper(mo_cert_store *certstore);

} //namespace MicroOcpp

#endif //__cplusplus

#endif //(MO_ENABLE_V16 || MO_ENABLE_V201) && MO_ENABLE_CERT_MGMT
#endif
