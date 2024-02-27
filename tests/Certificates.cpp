#include <MicroOcpp.h>
#include <MicroOcpp/Core/Connection.h>
#include "./catch2/catch.hpp"
#include "./helpers/testHelper.h"

#include <MicroOcpp/Core/Context.h>
#include <MicroOcpp/Operations/CustomOperation.h>
#include <MicroOcpp/Core/SimpleRequestFactory.h>

#include <MicroOcpp/Core/FilesystemAdapter.h>
#include <MicroOcpp/Core/FilesystemUtils.h>
#include <MicroOcpp/Core/Configuration.h>

#include <MicroOcpp/Model/Certificates/CertificateService.h>
#include <MicroOcpp/Model/Certificates/CertificateMbedTLS.h>

#if !MO_ENABLE_MBEDTLS
#error Certificates unit tests depend on MbedTLS
#endif


#define BASE_TIME "2023-01-01T00:00:00.000Z"

//ISRG Root X1
const char *root_cert = R"(-----BEGIN CERTIFICATE-----
MIIFazCCA1OgAwIBAgIRAIIQz7DSQONZRGPgu2OCiwAwDQYJKoZIhvcNAQELBQAw
TzELMAkGA1UEBhMCVVMxKTAnBgNVBAoTIEludGVybmV0IFNlY3VyaXR5IFJlc2Vh
cmNoIEdyb3VwMRUwEwYDVQQDEwxJU1JHIFJvb3QgWDEwHhcNMTUwNjA0MTEwNDM4
WhcNMzUwNjA0MTEwNDM4WjBPMQswCQYDVQQGEwJVUzEpMCcGA1UEChMgSW50ZXJu
ZXQgU2VjdXJpdHkgUmVzZWFyY2ggR3JvdXAxFTATBgNVBAMTDElTUkcgUm9vdCBY
MTCCAiIwDQYJKoZIhvcNAQEBBQADggIPADCCAgoCggIBAK3oJHP0FDfzm54rVygc
h77ct984kIxuPOZXoHj3dcKi/vVqbvYATyjb3miGbESTtrFj/RQSa78f0uoxmyF+
0TM8ukj13Xnfs7j/EvEhmkvBioZxaUpmZmyPfjxwv60pIgbz5MDmgK7iS4+3mX6U
A5/TR5d8mUgjU+g4rk8Kb4Mu0UlXjIB0ttov0DiNewNwIRt18jA8+o+u3dpjq+sW
T8KOEUt+zwvo/7V3LvSye0rgTBIlDHCNAymg4VMk7BPZ7hm/ELNKjD+Jo2FR3qyH
B5T0Y3HsLuJvW5iB4YlcNHlsdu87kGJ55tukmi8mxdAQ4Q7e2RCOFvu396j3x+UC
B5iPNgiV5+I3lg02dZ77DnKxHZu8A/lJBdiB3QW0KtZB6awBdpUKD9jf1b0SHzUv
KBds0pjBqAlkd25HN7rOrFleaJ1/ctaJxQZBKT5ZPt0m9STJEadao0xAH0ahmbWn
OlFuhjuefXKnEgV4We0+UXgVCwOPjdAvBbI+e0ocS3MFEvzG6uBQE3xDk3SzynTn
jh8BCNAw1FtxNrQHusEwMFxIt4I7mKZ9YIqioymCzLq9gwQbooMDQaHWBfEbwrbw
qHyGO0aoSCqI3Haadr8faqU9GY/rOPNk3sgrDQoo//fb4hVC1CLQJ13hef4Y53CI
rU7m2Ys6xt0nUW7/vGT1M0NPAgMBAAGjQjBAMA4GA1UdDwEB/wQEAwIBBjAPBgNV
HRMBAf8EBTADAQH/MB0GA1UdDgQWBBR5tFnme7bl5AFzgAiIyBpY9umbbjANBgkq
hkiG9w0BAQsFAAOCAgEAVR9YqbyyqFDQDLHYGmkgJykIrGF1XIpu+ILlaS/V9lZL
ubhzEFnTIZd+50xx+7LSYK05qAvqFyFWhfFQDlnrzuBZ6brJFe+GnY+EgPbk6ZGQ
3BebYhtF8GaV0nxvwuo77x/Py9auJ/GpsMiu/X1+mvoiBOv/2X/qkSsisRcOj/KK
NFtY2PwByVS5uCbMiogziUwthDyC3+6WVwW6LLv3xLfHTjuCvjHIInNzktHCgKQ5
ORAzI4JMPJ+GslWYHb4phowim57iaztXOoJwTdwJx4nLCgdNbOhdjsnvzqvHu7Ur
TkXWStAmzOVyyghqpZXjFaH3pO3JLF+l+/+sKAIuvtd7u+Nxe5AW0wdeRlN8NwdC
jNPElpzVmbUq4JUagEiuTDkHzsxHpFKVK7q4+63SM1N95R1NbdWhscdCb+ZAJzVc
oyi3B43njTOQ5yOf+1CceWxG1bQVs5ZufpsMljq4Ui0/1lvh+wjChP4kqKOJ2qxq
4RgqsahDYVvTH9w7jXbyLeiNdd8XM2w9U/t7y0Ff/9yi0GE44Za4rF2LN9d11TPA
mRGunUHBcnWEvgJBQl9nJEiU0Zsnvgc/ubhPgXRR4Xq37Z0j4r7g1SgEEzwxA57d
emyPxgcYxn/eR44/KJ4EBs+lVDR3veyJm+kXQ99b21/+jh5Xos1AnX5iItreGCc=
-----END CERTIFICATE-----
)";

//precomputed identifiers of root cert above, based on Open Certificate Status Protocol (OCSP)
const char *root_cert_hash_algorithm = "SHA256"; //algorithm used for the following hashes
const char *root_cert_hash_issuer_name = "998AC9E24B24D4008398DFA1375FD5D3DA7F2827E547ABFE5DB367B9E2CD5B20";
const char *root_cert_hash_issuer_key = "0B9FA5A59EED715C26C1020C711B4F6EC42D58B0015E14337A39DAD301C5AFC3";
const char *root_cert_hash_serial_number = "8210CFB0D240E3594463E0BB63828B00";

using namespace MicroOcpp;

TEST_CASE( "M - Certificates" ) {
    printf("\nRun %s\n",  "M - Certificates");

    //clean state
    auto filesystem = makeDefaultFilesystemAdapter(FilesystemOpt::Use_Mount_FormatOnFail);
    FilesystemUtils::remove_if(filesystem, [] (const char*) {return true;});

    //initialize Context with dummy socket
    LoopbackConnection loopback;

    mocpp_set_timer(custom_timer_cb);

    mocpp_initialize(loopback, ChargerCredentials("test-runner"));
    auto& model = getOcppContext()->getModel();
    auto certService = model.getCertificateService();
    SECTION("CertificateService initialized") {
        REQUIRE(certService != nullptr);
    }
    auto certs = certService->getCertificateStore();
    SECTION("CertificateStore initialized") {
        REQUIRE(certs != nullptr);
    }

    auto connector = model.getConnector(1);
    model.getClock().setTime(BASE_TIME);

    loop();

    SECTION("M05 Install CA cert -- sent cert is valid") {
        auto ret = certs->installCertificate(InstallCertificateType::CSMSRootCertificate, root_cert);
        REQUIRE(ret == InstallCertificateStatus::Accepted);

        size_t msize;
        char fn [MO_MAX_PATH_SIZE];
        printCertFn(MO_CERT_FN_CSMS_ROOT, 0, fn, MO_MAX_PATH_SIZE);
        REQUIRE(filesystem->stat(fn, &msize) == 0);
        REQUIRE(msize == strlen(root_cert));
    }

    SECTION("M03 Retrieve list of available certs -- one cert available") {
        auto ret1 = certs->installCertificate(InstallCertificateType::CSMSRootCertificate, root_cert);
        REQUIRE(ret1 == InstallCertificateStatus::Accepted);

        std::vector<CertificateChainHash> chain;
        auto ret2 = certs->getCertificateIds(GetCertificateIdType::CSMSRootCertificate, chain);

        REQUIRE(ret2 == GetInstalledCertificateStatus::Accepted);
        REQUIRE(chain.size() == 1);

        auto& chainElem = chain.front();

        REQUIRE(chainElem.certificateType == GetCertificateIdType::CSMSRootCertificate);
        auto& certHash = chainElem.certificateHashData;

        REQUIRE(!strcmp(certHash.getHashAlgorithmCStr(), root_cert_hash_algorithm)); //if this fails, please update the precomputed test hashes
        REQUIRE(!strcmp(certHash.getIssuerNameHash(), root_cert_hash_issuer_name));
        REQUIRE(!strcmp(certHash.getIssuerKeyHash(), root_cert_hash_issuer_key));
        REQUIRE(!strcmp(certHash.getSerialNumber(), root_cert_hash_serial_number));
        REQUIRE(chainElem.childCertificateHashData.empty()); //no sub certs sent
    }

    SECTION("M04 Delete a specific cert -- specified cert exists") {
        auto ret1 = certs->installCertificate(InstallCertificateType::CSMSRootCertificate, root_cert);
        REQUIRE(ret1 == InstallCertificateStatus::Accepted);

        std::vector<CertificateChainHash> chain;
        auto ret2 = certs->getCertificateIds(GetCertificateIdType::CSMSRootCertificate, chain);
        REQUIRE(ret2 == GetInstalledCertificateStatus::Accepted);

        REQUIRE(chain.size() == 1);

        auto ret3 = certs->deleteCertificate(chain.front().certificateHashData);
        REQUIRE(ret3 == DeleteCertificateStatus::Accepted);

        ret2 = certs->getCertificateIds(GetCertificateIdType::CSMSRootCertificate, chain);
        REQUIRE(ret2 == GetInstalledCertificateStatus::NotFound);

        REQUIRE(chain.size() == 0);

        size_t msize;
        char fn [MO_MAX_PATH_SIZE];
        printCertFn(MO_CERT_FN_CSMS_ROOT, 0, fn, MO_MAX_PATH_SIZE);
        REQUIRE(filesystem->stat(fn, &msize) != 0);
    }

    SECTION("M05 InstallCertificate operation") {

        bool checkProcessed = false;
        getOcppContext()->initiateRequest(makeRequest(new Ocpp16::CustomOperation(
                "InstallCertificate",
                [] () {
                    //create req
                    auto doc = std::unique_ptr<DynamicJsonDocument>(new DynamicJsonDocument(
                            JSON_OBJECT_SIZE(2)));
                    auto payload = doc->to<JsonObject>();
                    payload["certificateType"] = "CSMSRootCertificate"; //of InstallCertificateTypeEnumType
                    payload["certificate"] = root_cert;
                    return doc;},
                [&checkProcessed] (JsonObject payload) {
                    //receive conf
                    checkProcessed = true;

                    REQUIRE( !strcmp(payload["status"] | "_Undefined", "Accepted") );
                }
        )));
        loop();
        REQUIRE( checkProcessed );

        size_t msize;
        char fn [MO_MAX_PATH_SIZE];
        printCertFn(MO_CERT_FN_CSMS_ROOT, 0, fn, MO_MAX_PATH_SIZE);
        REQUIRE(filesystem->stat(fn, &msize) == 0);
        REQUIRE(msize == strlen(root_cert));
    }

    SECTION("M04 DeleteCertificate operation") {
        auto ret = certs->installCertificate(InstallCertificateType::CSMSRootCertificate, root_cert);
        REQUIRE(ret == InstallCertificateStatus::Accepted);

        bool checkProcessed = false;
        getOcppContext()->initiateRequest(makeRequest(new Ocpp16::CustomOperation(
                "DeleteCertificate",
                [] () {
                    //create req
                    auto doc = std::unique_ptr<DynamicJsonDocument>(new DynamicJsonDocument(
                            JSON_OBJECT_SIZE(4)));
                    auto payload = doc->to<JsonObject>();
                    payload["hashAlgorithm"] = root_cert_hash_algorithm; //of HashAlgorithmEnumType
                    payload["issuerNameHash"] = root_cert_hash_issuer_name;
                    payload["issuerKeyHash"] = root_cert_hash_issuer_key;
                    payload["serialNumber"] = root_cert_hash_serial_number;
                    return doc;},
                [&checkProcessed] (JsonObject payload) {
                    //receive conf
                    checkProcessed = true;

                    REQUIRE( !strcmp(payload["status"] | "_Undefined", "Accepted") );
                }
        )));
        loop();
        REQUIRE( checkProcessed );
    }

    SECTION("M03 GetInstalledCertificateIds operation") {
        auto ret = certs->installCertificate(InstallCertificateType::CSMSRootCertificate, root_cert);
        REQUIRE(ret == InstallCertificateStatus::Accepted);

        bool checkProcessed = false;
        getOcppContext()->initiateRequest(makeRequest(new Ocpp16::CustomOperation(
                "GetInstalledCertificateIds",
                [] () {
                    //create req
                    auto doc = std::unique_ptr<DynamicJsonDocument>(new DynamicJsonDocument(
                            JSON_OBJECT_SIZE(1)));
                    auto payload = doc->to<JsonObject>();
                    payload["certificateType"] = "CSMSRootCertificate"; //of GetCertificateIdTypeEnumType
                    return doc;},
                [&checkProcessed] (JsonObject payload) {
                    //receive conf
                    checkProcessed = true;

                    REQUIRE( !strcmp(payload["status"] | "_Undefined", "Accepted") );
                    REQUIRE( payload["certificateHashDataChain"].size() == 1 );
                    JsonObject certificateHashDataChain = payload["certificateHashDataChain"][0];
                    REQUIRE( !strcmp(certificateHashDataChain["certificateType"] | "_Undefined", "CSMSRootCertificate") );
                    JsonObject certificateHashData = certificateHashDataChain["certificateHashData"];
                    REQUIRE( !strcmp(certificateHashData["hashAlgorithm"] | "_Undefined", root_cert_hash_algorithm) ); //if this fails, please update the precomputed test hashes
                    REQUIRE( !strcmp(certificateHashData["issuerNameHash"] | "_Undefined", root_cert_hash_issuer_name) );
                    REQUIRE( !strcmp(certificateHashData["issuerKeyHash"] | "_Undefined", root_cert_hash_issuer_key) );
                    REQUIRE( !strcmp(certificateHashData["serialNumber"] | "_Undefined", root_cert_hash_serial_number) );
                    REQUIRE( !certificateHashDataChain.containsKey("childCertificateHashData") );
                }
        )));
        loop();
        REQUIRE( checkProcessed );
    }

    mocpp_deinitialize();
}
