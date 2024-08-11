// matth-x/MicroOcpp
// Copyright Matthias Akstaller 2019 - 2024
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

#if MO_ENABLE_CERT_MGMT

#include <functional>
#include <memory>

#include <MicroOcpp/Model/Certificates/Certificate.h>
#include <MicroOcpp/Core/Memory.h>

namespace MicroOcpp {

class Context;

class CertificateService : public AllocOverrider {
private:
    Context& context;
    std::unique_ptr<CertificateStore> certStore;
public:
    CertificateService(Context& context);

    void setCertificateStore(std::unique_ptr<CertificateStore> certStore);
    CertificateStore *getCertificateStore();
};

}

#endif //MO_ENABLE_CERT_MGMT
#endif
