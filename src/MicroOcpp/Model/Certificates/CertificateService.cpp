// matth-x/MicroOcpp
// Copyright Matthias Akstaller 2019 - 2024
// MIT License

#include <MicroOcpp/Model/Certificates/CertificateService.h>

#include <MicroOcpp/Context.h>
#include <MicroOcpp/Operations/DeleteCertificate.h>
#include <MicroOcpp/Operations/GetInstalledCertificateIds.h>
#include <MicroOcpp/Operations/InstallCertificate.h>
#include <MicroOcpp/Debug.h>

#if (MO_ENABLE_V16 || MO_ENABLE_V201) && MO_ENABLE_CERT_MGMT

using namespace MicroOcpp;

CertificateService::CertificateService(Context& context)
        : MemoryManaged("v16/v201.Certificates.CertificateService"), context(context) {

}

bool CertificateService::setup() {
    certStore = context.getCertificateStore();
    if (!certStore) {
        MO_DBG_WARN("CertMgmt disabled, because CertificateStore implementation is missing");
        return true; // not a critical failure
    }

    context.getMessageService().registerOperation("DeleteCertificate", [] (Context& context) -> Operation* {
        return new DeleteCertificate(*context.getModelCommon().getCertificateService());});
    context.getMessageService().registerOperation("GetInstalledCertificateIds", [] (Context& context) -> Operation* {
        return new GetInstalledCertificateIds(*context.getModelCommon().getCertificateService());});
    context.getMessageService().registerOperation("InstallCertificate", [] (Context& context) -> Operation* {
        return new InstallCertificate(*context.getModelCommon().getCertificateService());});

    return true;
}

CertificateStore *CertificateService::getCertificateStore() {
    return certStore;
}

#endif //(MO_ENABLE_V16 || MO_ENABLE_V201) && MO_ENABLE_CERT_MGMT
