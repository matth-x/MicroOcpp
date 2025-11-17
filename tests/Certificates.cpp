// matth-x/MicroOcpp
// Copyright Matthias Akstaller 2019 - 2024
// MIT License

#include <MicroOcpp/Platform.h>

//#if MO_ENABLE_MBEDTLS
#if 1

#include <MicroOcpp.h>
#include <catch2/catch.hpp>
#include "./helpers/testHelper.h"

#include <MicroOcpp/Context.h>

#include <MicroOcpp/Core/FilesystemUtils.h>

#include <MicroOcpp/Model/Certificates/CertificateService.h>
#include <MicroOcpp/Model/Certificates/CertificateMbedTLS.h>

#define BASE_TIME_UNIX 1750000000 //"2025-06-15T15:06:40Z"

//ISRG Root X1
#define ROOT_CERT_ENDL(ENDL) "-----BEGIN CERTIFICATE-----" ENDL \
"MIIFazCCA1OgAwIBAgIRAIIQz7DSQONZRGPgu2OCiwAwDQYJKoZIhvcNAQELBQAw" ENDL \
"TzELMAkGA1UEBhMCVVMxKTAnBgNVBAoTIEludGVybmV0IFNlY3VyaXR5IFJlc2Vh" ENDL \
"cmNoIEdyb3VwMRUwEwYDVQQDEwxJU1JHIFJvb3QgWDEwHhcNMTUwNjA0MTEwNDM4" ENDL \
"WhcNMzUwNjA0MTEwNDM4WjBPMQswCQYDVQQGEwJVUzEpMCcGA1UEChMgSW50ZXJu" ENDL \
"ZXQgU2VjdXJpdHkgUmVzZWFyY2ggR3JvdXAxFTATBgNVBAMTDElTUkcgUm9vdCBY" ENDL \
"MTCCAiIwDQYJKoZIhvcNAQEBBQADggIPADCCAgoCggIBAK3oJHP0FDfzm54rVygc" ENDL \
"h77ct984kIxuPOZXoHj3dcKi/vVqbvYATyjb3miGbESTtrFj/RQSa78f0uoxmyF+" ENDL \
"0TM8ukj13Xnfs7j/EvEhmkvBioZxaUpmZmyPfjxwv60pIgbz5MDmgK7iS4+3mX6U" ENDL \
"A5/TR5d8mUgjU+g4rk8Kb4Mu0UlXjIB0ttov0DiNewNwIRt18jA8+o+u3dpjq+sW" ENDL \
"T8KOEUt+zwvo/7V3LvSye0rgTBIlDHCNAymg4VMk7BPZ7hm/ELNKjD+Jo2FR3qyH" ENDL \
"B5T0Y3HsLuJvW5iB4YlcNHlsdu87kGJ55tukmi8mxdAQ4Q7e2RCOFvu396j3x+UC" ENDL \
"B5iPNgiV5+I3lg02dZ77DnKxHZu8A/lJBdiB3QW0KtZB6awBdpUKD9jf1b0SHzUv" ENDL \
"KBds0pjBqAlkd25HN7rOrFleaJ1/ctaJxQZBKT5ZPt0m9STJEadao0xAH0ahmbWn" ENDL \
"OlFuhjuefXKnEgV4We0+UXgVCwOPjdAvBbI+e0ocS3MFEvzG6uBQE3xDk3SzynTn" ENDL \
"jh8BCNAw1FtxNrQHusEwMFxIt4I7mKZ9YIqioymCzLq9gwQbooMDQaHWBfEbwrbw" ENDL \
"qHyGO0aoSCqI3Haadr8faqU9GY/rOPNk3sgrDQoo//fb4hVC1CLQJ13hef4Y53CI" ENDL \
"rU7m2Ys6xt0nUW7/vGT1M0NPAgMBAAGjQjBAMA4GA1UdDwEB/wQEAwIBBjAPBgNV" ENDL \
"HRMBAf8EBTADAQH/MB0GA1UdDgQWBBR5tFnme7bl5AFzgAiIyBpY9umbbjANBgkq" ENDL \
"hkiG9w0BAQsFAAOCAgEAVR9YqbyyqFDQDLHYGmkgJykIrGF1XIpu+ILlaS/V9lZL" ENDL \
"ubhzEFnTIZd+50xx+7LSYK05qAvqFyFWhfFQDlnrzuBZ6brJFe+GnY+EgPbk6ZGQ" ENDL \
"3BebYhtF8GaV0nxvwuo77x/Py9auJ/GpsMiu/X1+mvoiBOv/2X/qkSsisRcOj/KK" ENDL \
"NFtY2PwByVS5uCbMiogziUwthDyC3+6WVwW6LLv3xLfHTjuCvjHIInNzktHCgKQ5" ENDL \
"ORAzI4JMPJ+GslWYHb4phowim57iaztXOoJwTdwJx4nLCgdNbOhdjsnvzqvHu7Ur" ENDL \
"TkXWStAmzOVyyghqpZXjFaH3pO3JLF+l+/+sKAIuvtd7u+Nxe5AW0wdeRlN8NwdC" ENDL \
"jNPElpzVmbUq4JUagEiuTDkHzsxHpFKVK7q4+63SM1N95R1NbdWhscdCb+ZAJzVc" ENDL \
"oyi3B43njTOQ5yOf+1CceWxG1bQVs5ZufpsMljq4Ui0/1lvh+wjChP4kqKOJ2qxq" ENDL \
"4RgqsahDYVvTH9w7jXbyLeiNdd8XM2w9U/t7y0Ff/9yi0GE44Za4rF2LN9d11TPA" ENDL \
"mRGunUHBcnWEvgJBQl9nJEiU0Zsnvgc/ubhPgXRR4Xq37Z0j4r7g1SgEEzwxA57d" ENDL \
"emyPxgcYxn/eR44/KJ4EBs+lVDR3veyJm+kXQ99b21/+jh5Xos1AnX5iItreGCc=" ENDL \
"-----END CERTIFICATE-----" ENDL

#define ROOT_CERT      ROOT_CERT_ENDL("\n")
#define ROOT_CERT_JSON ROOT_CERT_ENDL("\\n")

//precomputed identifiers of root cert above, based on Open Certificate Status Protocol (OCSP)
#define ROOT_CERT_HASH_ALGORITHM "SHA256" //algorithm used for the following hashes
#define ROOT_CERT_HASH_ISSUER_NAME "F6DB2FBD9DD85D9259DDB3C6DE7D7B2FEC3F3E0CEF1761BCBF3320571E2D30F8"
#define ROOT_CERT_HASH_ISSUER_KEY "F4593A1E07CC9CCEFFBED9C11DC5218356F7814D9B22949DE745E629990C6C60"
#define ROOT_CERT_HASH_SERIAL_NUMBER "8210CFB0D240E3594463E0BB63828B00"

using namespace MicroOcpp;

TEST_CASE( "M - Certificates" ) {
    printf("\nRun %s\n",  "M - Certificates");

    //initialize Context with dummy socket
    mo_initialize();
    mo_useMockServer();

    auto ocppVersion = GENERATE(MO_OCPP_V16, MO_OCPP_V201);
    mo_setOcppVersion(ocppVersion);

    mo_setup();

    CertificateService *certService = nullptr;

    if (ocppVersion == MO_OCPP_V16) {
        certService =  mo_getContext()->getModel16().getCertificateService();
    } else if (ocppVersion == MO_OCPP_V201) {
        certService =  mo_getContext()->getModel201().getCertificateService();
    }

    REQUIRE(certService != nullptr);

    CertificateStore *certStore = certService->getCertificateStore();

    REQUIRE(certStore != nullptr);

    mo_setUnixTime(BASE_TIME_UNIX);

    //clean state
    auto filesystem = mo_getFilesystem();
    FilesystemUtils::removeByPrefix(filesystem, "");

    loop();

    SECTION("M05 Install CA cert -- sent cert is valid") {
        auto ret = certStore->installCertificate(InstallCertificateType_CSMSRootCertificate, ROOT_CERT);
        REQUIRE(ret == InstallCertificateStatus_Accepted);

        size_t msize;
        char path [MO_MAX_PATH_SIZE];
        printCertPath(filesystem, path, MO_MAX_PATH_SIZE, MO_CERT_FN_CSMS_ROOT, 0);
        REQUIRE(filesystem->stat(path, &msize) == 0);
        REQUIRE(msize == strlen(ROOT_CERT));
    }

    SECTION("M03 Retrieve list of available certs -- one cert available") {
        auto ret1 = certStore->installCertificate(InstallCertificateType_CSMSRootCertificate, ROOT_CERT);
        REQUIRE(ret1 == InstallCertificateStatus_Accepted);

        auto chain = makeVector<CertificateChainHash>("UnitTests");
        auto ret2 = certStore->getCertificateIds({GetCertificateIdType_CSMSRootCertificate}, chain);

        REQUIRE(ret2 == GetInstalledCertificateStatus_Accepted);
        REQUIRE(chain.size() == 1);

        auto& chainElem = chain.front();

        REQUIRE(chainElem.certificateType == GetCertificateIdType_CSMSRootCertificate);
        auto& certHash = chainElem.certificateHashData;

        REQUIRE(!strcmp(HashAlgorithmLabel(certHash.hashAlgorithm), ROOT_CERT_HASH_ALGORITHM)); //if this fails, please update the precomputed test hashes

        char buf [MO_CERT_HASH_ISSUER_NAME_KEY_SIZE];

        mo_cert_print_issuerNameHash(&certHash, buf, sizeof(buf));
        REQUIRE(!strcmp(buf, ROOT_CERT_HASH_ISSUER_NAME));

        mo_cert_print_issuerKeyHash(&certHash, buf, sizeof(buf));
        REQUIRE(!strcmp(buf, ROOT_CERT_HASH_ISSUER_KEY));

        mo_cert_print_serialNumber(&certHash, buf, sizeof(buf));
        REQUIRE(!strcmp(buf, ROOT_CERT_HASH_SERIAL_NUMBER));

        REQUIRE(chainElem.childCertificateHashData.empty()); //no sub certs sent
    }

    SECTION("M04 Delete a specific cert -- specified cert exists") {
        auto ret1 = certStore->installCertificate(InstallCertificateType_CSMSRootCertificate, ROOT_CERT);
        REQUIRE(ret1 == InstallCertificateStatus_Accepted);

        auto chain = makeVector<CertificateChainHash>("UnitTests");
        auto ret2 = certStore->getCertificateIds({GetCertificateIdType_CSMSRootCertificate}, chain);
        REQUIRE(ret2 == GetInstalledCertificateStatus_Accepted);

        REQUIRE(chain.size() == 1);

        auto ret3 = certStore->deleteCertificate(chain.front().certificateHashData);
        REQUIRE(ret3 == DeleteCertificateStatus_Accepted);

        ret2 = certStore->getCertificateIds({GetCertificateIdType_CSMSRootCertificate}, chain);
        REQUIRE(ret2 == GetInstalledCertificateStatus_NotFound);

        REQUIRE(chain.size() == 0);

        size_t msize;
        char path [MO_MAX_PATH_SIZE];
        printCertPath(filesystem, path, MO_MAX_PATH_SIZE, MO_CERT_FN_CSMS_ROOT, 0);
        REQUIRE(filesystem->stat(path, &msize) != 0);
    }

    SECTION("M05 InstallCertificate operation") {

        bool checkProcessed = false;
    
        mo_sendRequest(mo_getApiContext(),
                "InstallCertificate",
                "{\"certificateType\":\"CSMSRootCertificate\",\"certificate\":\"" ROOT_CERT_JSON "\"}",
                [] (const char *payloadJson, void *userData) {
                    //receive conf
                    bool *checkProcessed = reinterpret_cast<bool*>(userData);
                    *checkProcessed = true;

                    StaticJsonDocument<256> payload;
                    deserializeJson(payload, payloadJson);
                    REQUIRE( !strcmp(payload["status"] | "_Undefined", "Accepted") );
                }, nullptr, reinterpret_cast<void*>(&checkProcessed));
        loop();
        REQUIRE( checkProcessed );

        size_t msize;
        char path [MO_MAX_PATH_SIZE];
        printCertPath(filesystem, path, MO_MAX_PATH_SIZE, MO_CERT_FN_CSMS_ROOT, 0);
        REQUIRE(filesystem->stat(path, &msize) == 0);
        REQUIRE(msize == strlen(ROOT_CERT));
    }

    SECTION("M04 DeleteCertificate operation") {
        auto ret = certStore->installCertificate(InstallCertificateType_CSMSRootCertificate, ROOT_CERT);
        REQUIRE(ret == InstallCertificateStatus_Accepted);

        bool checkProcessed = false;

        mo_sendRequest(mo_getApiContext(),
                "DeleteCertificate",
                "{"
                    "\"certificateHashData\": {"
                        "\"hashAlgorithm\":\"" ROOT_CERT_HASH_ALGORITHM "\","
                        "\"issuerNameHash\":\"" ROOT_CERT_HASH_ISSUER_NAME "\","
                        "\"issuerKeyHash\":\"" ROOT_CERT_HASH_ISSUER_KEY "\","
                        "\"serialNumber\":\"" ROOT_CERT_HASH_SERIAL_NUMBER "\""
                    "}"
                "}",
                [] (const char *payloadJson, void *userData) {
                    //receive conf
                    bool *checkProcessed = reinterpret_cast<bool*>(userData);
                    *checkProcessed = true;

                    StaticJsonDocument<256> payload;
                    deserializeJson(payload, payloadJson);
                    REQUIRE( !strcmp(payload["status"] | "_Undefined", "Accepted") );
                }, nullptr, reinterpret_cast<void*>(&checkProcessed));

        loop();
        REQUIRE( checkProcessed );
    }

    SECTION("M03 GetInstalledCertificateIds operation") {
        auto ret = certStore->installCertificate(InstallCertificateType_CSMSRootCertificate, ROOT_CERT);
        REQUIRE(ret == InstallCertificateStatus_Accepted);

        bool checkProcessed = false;
        mo_sendRequest(mo_getApiContext(),
                "GetInstalledCertificateIds",
                "{\"certificateType\":[\"CSMSRootCertificate\"]}",
                [] (const char *payloadJson, void *userData) {
                    //receive conf
                    bool *checkProcessed = reinterpret_cast<bool*>(userData);
                    *checkProcessed = true;

                    StaticJsonDocument<1024> payload;
                    deserializeJson(payload, payloadJson);

                    REQUIRE( !strcmp(payload["status"] | "_Undefined", "Accepted") );
                    REQUIRE( payload["certificateHashDataChain"].size() == 1 );
                    JsonObject certificateHashDataChain = payload["certificateHashDataChain"][0];
                    REQUIRE( !strcmp(certificateHashDataChain["certificateType"] | "_Undefined", "CSMSRootCertificate") );
                    JsonObject certificateHashData = certificateHashDataChain["certificateHashData"];
                    REQUIRE( !strcmp(certificateHashData["hashAlgorithm"] | "_Undefined", ROOT_CERT_HASH_ALGORITHM) ); //if this fails, please update the precomputed test hashes
                    REQUIRE( !strcmp(certificateHashData["issuerNameHash"] | "_Undefined", ROOT_CERT_HASH_ISSUER_NAME) );
                    REQUIRE( !strcmp(certificateHashData["issuerKeyHash"] | "_Undefined", ROOT_CERT_HASH_ISSUER_KEY) );
                    REQUIRE( !strcmp(certificateHashData["serialNumber"] | "_Undefined", ROOT_CERT_HASH_SERIAL_NUMBER) );
                    REQUIRE( !certificateHashDataChain.containsKey("childCertificateHashData") );
                }, nullptr, reinterpret_cast<void*>(&checkProcessed));

        loop();
        REQUIRE( checkProcessed );
    }

    mo_deinitialize();
}

#else
#warning Certificates unit tests depend on MbedTLS
#endif //MO_ENABLE_MBEDTLS
