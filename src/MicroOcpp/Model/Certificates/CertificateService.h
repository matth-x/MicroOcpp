// matth-x/MicroOcpp
// Copyright Matthias Akstaller 2019 - 2025
// MIT License

/*
 * Functional Block M: ISO 15118 Certificate Management
 *
 * Implementation of UC:
 *     - M03
 *     - M04
 *     - M05
 */

#ifndef MO_CERTIFICATESERVICE_H
#define MO_CERTIFICATESERVICE_H

#include <MicroOcpp/Version.h>

#if (MO_ENABLE_V16 || MO_ENABLE_V201) && MO_ENABLE_CERT_MGMT

#ifdef __cplusplus
#include <functional>
#include <memory>
#endif //__cplusplus

#include <MicroOcpp/Model/Certificates/Certificate.h>
#include <MicroOcpp/Core/Memory.h>

#ifdef __cplusplus

namespace MicroOcpp {

class Context;

class CertificateService : public MemoryManaged {
private:
    Context& context;
    CertificateStore *certStore = nullptr;
public:
    CertificateService(Context& context);

    bool setup();

    CertificateStore *getCertificateStore();
};

}

#endif //__cplusplus
#endif //(MO_ENABLE_V16 || MO_ENABLE_V201) && MO_ENABLE_CERT_MGMT
#endif
