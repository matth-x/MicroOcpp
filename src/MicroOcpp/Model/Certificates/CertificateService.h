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

#include <functional>
#include <memory>

#include <MicroOcpp/Model/Certificates/Certificate.h>

namespace MicroOcpp {

class Context;

class CertificateService {
private:
    Context& context;
    std::unique_ptr<CertificateStore> certStore;
public:
    CertificateService(Context& context, std::unique_ptr<CertificateStore> certStore);

    CertificateStore *getCertificateStore();
};

}

#endif
