#ifndef MO_CERTIFICATE_MBEDTLS_H
#define MO_CERTIFICATE_MBEDTLS_H

/*
 * Built-in implementation of the Certificate interface for MbedTLS
 */

#ifndef MO_ENABLE_MBEDTLS
#define MO_ENABLE_MBEDTLS 0
#endif

#if MO_ENABLE_MBEDTLS

/*
 * Provide certificate interpreter to facilitate cert store in C. A full implementation is only available for C++
 */
#include <MicroOcpp/Model/Certificates/Certificate_c.h>

#ifdef __cplusplus
extern "C" {
#endif

bool ocpp_get_cert_hash(const unsigned char *cert, size_t len, enum HashAlgorithmEnumType_c hashAlg, ocpp_certificate_hash *out);

#ifdef __cplusplus
} //extern "C"

#include <memory>

#include <MicroOcpp/Model/Certificates/Certificate.h>
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
#define MO_CERT_FN_MANUFACTURER_ROOT "mfact"
#endif

#ifndef MO_CERT_STORE_SIZE
#define MO_CERT_STORE_SIZE 3 //max number of certs per certificate type (e.g. CSMS root CA, Manufacturer root CA)
#endif

namespace MicroOcpp {

std::unique_ptr<CertificateStore> makeCertificateStoreMbedTLS(std::shared_ptr<FilesystemAdapter> filesystem);

bool printCertFn(const char *certType, size_t index, char *buf, size_t bufsize);

} //namespace MicroOcpp

#endif //def __cplusplus
#endif //MO_ENABLE_MBEDTLS

#endif
