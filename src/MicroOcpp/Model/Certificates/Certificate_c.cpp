// matth-x/MicroOcpp
// Copyright Matthias Akstaller 2019 - 2024
// MIT License

#include <MicroOcpp/Model/Certificates/Certificate_c.h>

#if (MO_ENABLE_V16 || MO_ENABLE_V201) && MO_ENABLE_CERT_MGMT

#include <MicroOcpp/Core/Memory.h>
#include <MicroOcpp/Debug.h>

#include <string.h>

namespace MicroOcpp {

/*
 * C++ wrapper for the C-style certificate interface
 */
class CertificateStoreC : public CertificateStore, public MemoryManaged {
private:
    mo_cert_store *certstore = nullptr;
public:
    CertificateStoreC(mo_cert_store *certstore) : MemoryManaged("v201.Certificates.CertificateStoreC"), certstore(certstore) {

    }

    ~CertificateStoreC() = default;

    GetInstalledCertificateStatus getCertificateIds(const Vector<GetCertificateIdType>& certificateType, Vector<CertificateChainHash>& out) override {
        out.clear();

        mo_cert_chain_hash *cch;

        auto ret = certstore->getCertificateIds(certstore->user_data, &certificateType[0], certificateType.size(), &cch);
        if (ret == GetInstalledCertificateStatus_NotFound || !cch) {
            return GetInstalledCertificateStatus_NotFound;
        }

        bool err = false;
        
        for (mo_cert_chain_hash *it = cch; it && !err; it = it->next) {
            out.emplace_back();
            auto &chd_el = out.back();
            chd_el.certificateType = it->certType;
            memcpy(&chd_el.certificateHashData, &it->certHashData, sizeof(mo_cert_hash));
        }

        while (cch) {
            mo_cert_chain_hash *el = cch;
            cch = cch->next;
            el->invalidate(el);
        }

        if (err) {
            out.clear();
        }

        return out.empty() ?
                GetInstalledCertificateStatus_NotFound :
                GetInstalledCertificateStatus_Accepted;
    }

    DeleteCertificateStatus deleteCertificate(const CertificateHash& hash) override {
        return certstore->deleteCertificate(certstore->user_data, &hash);
    }

    InstallCertificateStatus installCertificate(InstallCertificateType certificateType, const char *certificate) override {
        return certstore->installCertificate(certstore->user_data, certificateType, certificate);
    }
};

std::unique_ptr<CertificateStore> makeCertificateStoreCwrapper(mo_cert_store *certstore) {
    return std::unique_ptr<CertificateStore>(new CertificateStoreC(certstore));
}

} //namespace MicroOcpp
#endif //(MO_ENABLE_V16 || MO_ENABLE_V201) && MO_ENABLE_CERT_MGMT
