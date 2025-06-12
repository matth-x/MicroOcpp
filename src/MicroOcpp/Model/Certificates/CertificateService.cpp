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

    #if MO_ENABLE_V16
    if (context.getOcppVersion() == MO_OCPP_V16) {
        context.getMessageService().registerOperation("DeleteCertificate", [] (Context& context) -> Operation* {
            return new DeleteCertificate(*context.getModel16().getCertificateService());});
        context.getMessageService().registerOperation("GetInstalledCertificateIds", [] (Context& context) -> Operation* {
            return new GetInstalledCertificateIds(*context.getModel16().getCertificateService());});
        context.getMessageService().registerOperation("InstallCertificate", [] (Context& context) -> Operation* {
            return new InstallCertificate(*context.getModel16().getCertificateService());});
    }
    #endif
    #if MO_ENABLE_V201
    if (context.getOcppVersion() == MO_OCPP_V201) {
        context.getMessageService().registerOperation("DeleteCertificate", [] (Context& context) -> Operation* {
            return new DeleteCertificate(*context.getModel201().getCertificateService());});
        context.getMessageService().registerOperation("GetInstalledCertificateIds", [] (Context& context) -> Operation* {
            return new GetInstalledCertificateIds(*context.getModel201().getCertificateService());});
        context.getMessageService().registerOperation("InstallCertificate", [] (Context& context) -> Operation* {
            return new InstallCertificate(*context.getModel201().getCertificateService());});
    }
    #endif

    return true;
}

CertificateStore *CertificateService::getCertificateStore() {
    return certStore;
}

#endif //(MO_ENABLE_V16 || MO_ENABLE_V201) && MO_ENABLE_CERT_MGMT
