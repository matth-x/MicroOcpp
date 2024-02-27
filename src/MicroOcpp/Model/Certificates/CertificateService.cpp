#include <MicroOcpp/Model/Certificates/CertificateService.h>
#include <MicroOcpp/Core/Context.h>
#include <MicroOcpp/Operations/DeleteCertificate.h>
#include <MicroOcpp/Operations/GetInstalledCertificateIds.h>
#include <MicroOcpp/Operations/InstallCertificate.h>

using namespace MicroOcpp;

CertificateService::CertificateService(Context& context, std::unique_ptr<CertificateStore> certStore)
        : context(context), certStore(std::move(certStore)) {

    context.getOperationRegistry().registerOperation("DeleteCertificate", [this] () {
        return new Ocpp201::DeleteCertificate(*this);});
    context.getOperationRegistry().registerOperation("GetInstalledCertificateIds", [this] () {
        return new Ocpp201::GetInstalledCertificateIds(*this);});
    context.getOperationRegistry().registerOperation("InstallCertificate", [this] () {
        return new Ocpp201::InstallCertificate(*this);});
}

CertificateStore *CertificateService::getCertificateStore() {
    return certStore.get();
}
