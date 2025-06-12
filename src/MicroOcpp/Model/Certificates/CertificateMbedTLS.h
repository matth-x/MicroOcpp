// matth-x/MicroOcpp
// Copyright Matthias Akstaller 2019 - 2024
// MIT License

#ifndef MO_CERTIFICATE_MBEDTLS_H
#define MO_CERTIFICATE_MBEDTLS_H

/*
 * Built-in implementation of the Certificate interface for MbedTLS
 */

#include <MicroOcpp/Version.h>
#include <MicroOcpp/Platform.h>

#ifndef MO_ENABLE_CERT_STORE_MBEDTLS
#define MO_ENABLE_CERT_STORE_MBEDTLS MO_ENABLE_MBEDTLS
#endif

#if (MO_ENABLE_V16 || MO_ENABLE_V201) && MO_ENABLE_CERT_MGMT && MO_ENABLE_CERT_STORE_MBEDTLS

/*
 * Provide certificate interpreter to facilitate cert store in C. A full implementation is only available for C++
 */
#include <MicroOcpp/Model/Certificates/Certificate.h>

#ifdef __cplusplus
extern "C" {
#endif

bool mo_get_cert_hash(const unsigned char *cert, size_t len, enum HashAlgorithmType hashAlg, mo_cert_hash *out);

#ifdef __cplusplus
} //extern "C"

#include <memory>

#include <MicroOcpp/Core/FilesystemAdapter.h>

#ifndef MO_CERT_FN_PREFIX
#define MO_CERT_FN_PREFIX "cert-"
#endif

#ifndef MO_CERT_FN_SUFFIX
#define MO_CERT_FN_SUFFIX ".pem"
#endif

#ifndef MO_CERT_FN_CSMS_ROOT
#define MO_CERT_FN_CSMS_ROOT "csms"
#endif

#ifndef MO_CERT_FN_MANUFACTURER_ROOT
#define MO_CERT_FN_MANUFACTURER_ROOT "mfct"
#endif

#ifndef MO_CERT_STORE_SIZE
#define MO_CERT_STORE_SIZE 3 //max number of certs per certificate type (e.g. CSMS root CA, Manufacturer root CA)
#endif

#ifndef MO_CERT_STORE_DIGITS
#define MO_CERT_STORE_DIGITS 1 //digits needed to print MO_CERT_STORE_SIZE-1 (="3", i.e. 1 digit)
#endif

namespace MicroOcpp {

std::unique_ptr<CertificateStore> makeCertificateStoreMbedTLS(MO_FilesystemAdapter *filesystem);

bool printCertPath(MO_FilesystemAdapter *filesystem, char *path, size_t size, const char *certType, unsigned int index);

} //namespace MicroOcpp

#endif //def __cplusplus
#endif //(MO_ENABLE_V16 || MO_ENABLE_V201) && MO_ENABLE_CERT_MGMT && MO_ENABLE_CERT_STORE_MBEDTLS

#endif
