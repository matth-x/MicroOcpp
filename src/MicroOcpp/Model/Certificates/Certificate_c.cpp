#include <MicroOcpp/Model/Certificates/Certificate_c.h>
#include <MicroOcpp/Debug.h>

namespace MicroOcpp {

/*
 * C++ wrapper for the C-style certificate interface
 */
class CertificateStoreC : public CertificateStore {
private:
    ocpp_certificate_store *certstore = nullptr;
public:
    CertificateStoreC(ocpp_certificate_store *certstore) : certstore(certstore) {

    }

    ~CertificateStoreC() = default;

    GetInstalledCertificateStatus getCertificateIds(GetCertificateIdType certificateType, std::vector<CertificateChainHash>& out) override {
        GetCertificateIdType_c ct_c;
        switch (certificateType) {
            case GetCertificateIdType::V2GRootCertificate:
                ct_c = ENUM_GCI_V2GRootCertificate;
                break;
            case GetCertificateIdType::MORootCertificate:
                ct_c = ENUM_GCI_MORootCertificate;
                break;
            case GetCertificateIdType::CSMSRootCertificate:
                ct_c = ENUM_GCI_CSMSRootCertificate;
                break;
            case GetCertificateIdType::V2GCertificateChain:
                ct_c = ENUM_GCI_V2GCertificateChain;
                break;
            case GetCertificateIdType::ManufacturerRootCertificate:
                ct_c = ENUM_GCI_ManufacturerRootCertificate;
                break;
            default:
                MO_DBG_ERR("internal error");
                return GetInstalledCertificateStatus::NotFound;
        }

        ocpp_certificate_chain_hash *cch;

        auto ret = certstore->getCertificateIds(certstore->user_data, ct_c, &cch);
        if (ret == ENUM_GICS_NotFound || !cch) {
            return GetInstalledCertificateStatus::NotFound;
        }

        bool err = false;
        
        for (ocpp_certificate_chain_hash *it = cch; it && !err; it = it->next) {
            out.emplace_back();
            auto &chd_el = out.back();
            switch (it->certificateType) {
                case ENUM_GCI_V2GRootCertificate:
                    chd_el.certificateType = GetCertificateIdType::V2GRootCertificate;
                    break;
                case ENUM_GCI_MORootCertificate:
                    chd_el.certificateType = GetCertificateIdType::MORootCertificate;
                    break;
                case ENUM_GCI_CSMSRootCertificate:
                    chd_el.certificateType = GetCertificateIdType::CSMSRootCertificate;
                    break;
                case ENUM_GCI_V2GCertificateChain:
                    chd_el.certificateType = GetCertificateIdType::V2GCertificateChain;
                    break;
                case ENUM_GCI_ManufacturerRootCertificate:
                    chd_el.certificateType = GetCertificateIdType::ManufacturerRootCertificate;
                    break;
                default:
                    MO_DBG_ERR("internal error");
                    err = true;
                    break;
            }

            switch (it->certificateHashData.hashAlgorithm) {
                case ENUM_HA_SHA256:
                    chd_el.certificateHashData.hashAlgorithm = HashAlgorithmEnumType::SHA256;
                    break;
                case ENUM_HA_SHA384:
                    chd_el.certificateHashData.hashAlgorithm = HashAlgorithmEnumType::SHA384;
                    break;
                case ENUM_HA_SHA512:
                    chd_el.certificateHashData.hashAlgorithm = HashAlgorithmEnumType::SHA512;
                    break;
                default:
                    MO_DBG_ERR("internal error");
                    err = true;
                    break;
            }

            bool success = true;
            int ret;
            ret = snprintf(chd_el.certificateHashData.issuerNameHash, sizeof(chd_el.certificateHashData.issuerNameHash), "%s", it->certificateHashData.issuerNameHash);
            success &= ret >= 0 && ret < sizeof(chd_el.certificateHashData.issuerNameHash);
            ret = snprintf(chd_el.certificateHashData.issuerKeyHash, sizeof(chd_el.certificateHashData.issuerKeyHash), "%s", it->certificateHashData.issuerKeyHash);
            success &= ret >= 0 && ret < sizeof(chd_el.certificateHashData.issuerKeyHash);
            ret = snprintf(chd_el.certificateHashData.serialNumber, sizeof(chd_el.certificateHashData.serialNumber), "%s", it->certificateHashData.serialNumber);
            success &= ret >= 0 && ret < sizeof(chd_el.certificateHashData.serialNumber);

            if (!success) {
                MO_DBG_ERR("error copying C-style struct");
                err = true;
            }
        }

        while (cch) {
            ocpp_certificate_chain_hash *el = cch;
            cch = cch->next;
            el->invalidate(el);
        }

        if (err) {
            out.clear();
        }

        return out.empty() ?
                GetInstalledCertificateStatus::NotFound :
                GetInstalledCertificateStatus::Accepted;
    }

    DeleteCertificateStatus deleteCertificate(const CertificateHash& hash) override {
        ocpp_certificate_hash ch;

        HashAlgorithmEnumType_c ha;

        switch (hash.hashAlgorithm) {
            case HashAlgorithmEnumType::SHA256:
                ha = ENUM_HA_SHA256;
                break;
            case HashAlgorithmEnumType::SHA384:
                ha = ENUM_HA_SHA384;
                break;
            case HashAlgorithmEnumType::SHA512:
                ha = ENUM_HA_SHA512;
                break;
            default:
                MO_DBG_ERR("internal error");
                return DeleteCertificateStatus::Failed;
        }

        ch.hashAlgorithm = ha;

        bool success = true;
        int ret;
        ret = snprintf(ch.issuerNameHash, sizeof(ch.issuerNameHash), "%s", hash.issuerNameHash);
        success &= ret >= 0 && ret < sizeof(ch.issuerNameHash);
        ret = snprintf(ch.issuerKeyHash, sizeof(ch.issuerKeyHash), "%s", hash.issuerKeyHash);
        success &= ret >= 0 && ret < sizeof(ch.issuerKeyHash);
        ret = snprintf(ch.serialNumber, sizeof(ch.serialNumber), "%s", hash.serialNumber);
        success &= ret >= 0 && ret < sizeof(ch.serialNumber);
        if (!success) {
            MO_DBG_ERR("error copying C-style struct");
            return DeleteCertificateStatus::Failed;
        }

        auto status = certstore->deleteCertificate(certstore->user_data, &ch);

        DeleteCertificateStatus dcs;

        switch (status) {
            case ENUM_DCS_Accepted:
                dcs = DeleteCertificateStatus::Accepted;
            case ENUM_DCS_Failed:
                dcs = DeleteCertificateStatus::Failed;
            case ENUM_DCS_NotFound:
                dcs = DeleteCertificateStatus::NotFound;
            default:
                MO_DBG_ERR("could not convert status type");
                return DeleteCertificateStatus::Failed; 
        }

        return dcs;
    }

    InstallCertificateStatus installCertificate(InstallCertificateType certificateType, const char *certificate) override {
        InstallCertificateType_c ic;
        
        switch (certificateType) {
            case InstallCertificateType::V2GRootCertificate:
                ic = ENUM_IC_V2GRootCertificate;
                break;
            case InstallCertificateType::MORootCertificate:
                ic = ENUM_IC_MORootCertificate;
                break;
            case InstallCertificateType::CSMSRootCertificate:
                ic = ENUM_IC_CSMSRootCertificate;
                break;
            case InstallCertificateType::ManufacturerRootCertificate:
                ic = ENUM_IC_ManufacturerRootCertificate;
                break;
            default:
                MO_DBG_ERR("internal error");
                return InstallCertificateStatus::Failed;
        }

        auto ret = certstore->installCertificate(certstore->user_data, ic, certificate);

        InstallCertificateStatus ics;

        switch (ret) {
            case ENUM_ICS_Accepted:
                ics = InstallCertificateStatus::Accepted;
                break;
            case ENUM_ICS_Rejected:
                ics = InstallCertificateStatus::Rejected;
                break;
            case ENUM_ICS_Failed:
                ics = InstallCertificateStatus::Failed;
                break;
            default:
                MO_DBG_ERR("could not convert status type");
                return InstallCertificateStatus::Rejected; 
        }

        return ics;
    }
};

std::unique_ptr<CertificateStore> makeCertificateStoreCwrapper(ocpp_certificate_store *certstore) {
    return std::unique_ptr<CertificateStore>(new CertificateStoreC(certstore));
}

} //namespace MicroOcpp
