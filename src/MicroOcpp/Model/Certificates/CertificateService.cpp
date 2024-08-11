// matth-x/MicroOcpp
// Copyright Matthias Akstaller 2019 - 2024
// MIT License

#include <MicroOcpp/Model/Certificates/CertificateService.h>

#if MO_ENABLE_CERT_MGMT

#include <MicroOcpp/Core/Context.h>
#include <MicroOcpp/Operations/DeleteCertificate.h>
#include <MicroOcpp/Operations/GetInstalledCertificateIds.h>
#include <MicroOcpp/Operations/InstallCertificate.h>

using namespace MicroOcpp;

CertificateService::CertificateService(Context& context)
        : AllocOverrider("v201.Certificates.CertificateService"), context(context) {

    context.getOperationRegistry().registerOperation("DeleteCertificate", [this] () {
        return new Ocpp201::DeleteCertificate(*this);});
    context.getOperationRegistry().registerOperation("GetInstalledCertificateIds", [this] () {
        return new Ocpp201::GetInstalledCertificateIds(*this);});
    context.getOperationRegistry().registerOperation("InstallCertificate", [this] () {
        return new Ocpp201::InstallCertificate(*this);});
}

void CertificateService::setCertificateStore(std::unique_ptr<CertificateStore> certStore) {
    this->certStore = std::move(certStore);
}

CertificateStore *CertificateService::getCertificateStore() {
    return certStore.get();
}

#endif //MO_ENABLE_CERT_MGMT
