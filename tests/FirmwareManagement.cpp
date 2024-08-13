// matth-x/MicroOcpp
// Copyright Matthias Akstaller 2019 - 2024
// MIT License

#include <MicroOcpp.h>
#include <MicroOcpp/Core/Connection.h>
#include <catch2/catch.hpp>
#include "./helpers/testHelper.h"

#include <MicroOcpp/Core/Context.h>
#include <MicroOcpp/Operations/CustomOperation.h>
#include <MicroOcpp/Core/Request.h>

#include <MicroOcpp/Core/FilesystemAdapter.h>
#include <MicroOcpp/Core/FilesystemUtils.h>
#include <MicroOcpp/Core/Configuration.h>

#include <MicroOcpp/Model/FirmwareManagement/FirmwareService.h>

#define BASE_TIME     "2023-01-01T00:00:00.000Z"
#define BASE_TIME_1H  "2023-01-01T01:00:00.000Z"
#define FTP_URL       "ftps://localhost/firmware.bin"

using namespace MicroOcpp;

TEST_CASE( "FirmwareManagement" ) {
    printf("\nRun %s\n",  "FirmwareManagement");

    //clean state
    auto filesystem = makeDefaultFilesystemAdapter(FilesystemOpt::Use_Mount_FormatOnFail);
    FilesystemUtils::remove_if(filesystem, [] (const char*) {return true;});

    //initialize Context with dummy socket
    LoopbackConnection loopback;

    mocpp_set_timer(custom_timer_cb);

    mocpp_initialize(loopback, ChargerCredentials("test-runner"));
    auto& model = getOcppContext()->getModel();
    auto fwService = getFirmwareService();
    SECTION("FirmwareService initialized") {
        REQUIRE(fwService != nullptr);
    }

    model.getClock().setTime(BASE_TIME);

    loop();

    SECTION("Unconfigured FW service") {

        bool checkProcessed = false;

        getOcppContext()->getOperationRegistry().registerOperation("FirmwareStatusNotification",
            [&checkProcessed] () {
                return new Ocpp16::CustomOperation("FirmwareStatusNotification",
                    [ &checkProcessed] (JsonObject payload) {
                        //process req
                        checkProcessed = true;
                        REQUIRE((
                            !strcmp(payload["status"] | "_Undefined", "DownloadFailed") ||
                            !strcmp(payload["status"] | "_Undefined", "InstallationFailed")
                        ));
                    },
                    [] () {
                        //create conf
                        return createEmptyDocument();
                    });
            });
        
        getOcppContext()->initiateRequest(makeRequest(new Ocpp16::CustomOperation(
                "UpdateFirmware",
                [] () {
                    //create req
                    auto doc = makeJsonDoc("UnitTests", JSON_OBJECT_SIZE(4));
                    auto payload = doc->to<JsonObject>();
                    payload["location"] = FTP_URL;
                    payload["retries"] = 1;
                    payload["retrieveDate"] = BASE_TIME;
                    payload["retryInterval"] = 1;
                    return doc;},
                [] (JsonObject) { } //ignore conf
        )));

        loop();

        REQUIRE(checkProcessed);
    }

    SECTION("Download phase only") {

        int checkProcessed = 0;

        getOcppContext()->getOperationRegistry().registerOperation("FirmwareStatusNotification",
            [&checkProcessed] () {
                return new Ocpp16::CustomOperation("FirmwareStatusNotification",
                    [ &checkProcessed] (JsonObject payload) {
                        //process req
                        if (checkProcessed == 0) {
                            REQUIRE( !strcmp(payload["status"] | "_Undefined", "Downloading") );
                            checkProcessed++;
                        } else if (checkProcessed == 1) {
                            REQUIRE( !strcmp(payload["status"] | "_Undefined", "Downloaded") );
                            checkProcessed++;
                        } else if (checkProcessed == 2) {
                            REQUIRE( !strcmp(payload["status"] | "_Undefined", "Installed") );
                            checkProcessed++;
                        }
                    },
                    [] () {
                        //create conf
                        return createEmptyDocument();
                    });
            });

        bool checkProcessedOnDownload = false;

        fwService->setOnDownload([&checkProcessedOnDownload] (const char *location) {
            checkProcessedOnDownload = true;
            return true;
        });

        int checkProcessedOnDownloadStatus = 0;

        fwService->setDownloadStatusInput([&checkProcessed, &checkProcessedOnDownloadStatus] () {
            if (checkProcessed == 0) {
                if (checkProcessedOnDownloadStatus == 0) checkProcessedOnDownloadStatus = 1;
                return DownloadStatus::NotDownloaded;
            } else {
                if (checkProcessedOnDownloadStatus == 1) checkProcessedOnDownloadStatus = 2;
                return DownloadStatus::Downloaded;
            }
        });

        getOcppContext()->initiateRequest(makeRequest(new Ocpp16::CustomOperation(
                "UpdateFirmware",
                [] () {
                    //create req
                    auto doc = makeJsonDoc("UnitTests", JSON_OBJECT_SIZE(4));
                    auto payload = doc->to<JsonObject>();
                    payload["location"] = FTP_URL;
                    payload["retries"] = 1;
                    payload["retrieveDate"] = BASE_TIME;
                    payload["retryInterval"] = 1;
                    return doc;},
                [] (JsonObject) { } //ignore conf
        )));

        for (unsigned int i = 0; i < 10; i++) {
            loop();
            mtime += 5000;
        }

        REQUIRE( checkProcessed == 3 ); //all FirmwareStatusNotification messages have been received
        REQUIRE( checkProcessedOnDownload );
        REQUIRE( checkProcessedOnDownloadStatus == 2 );
    }

    SECTION("Installation phase only") {

        int checkProcessed = 0;

        getOcppContext()->getOperationRegistry().registerOperation("FirmwareStatusNotification",
            [&checkProcessed] () {
                return new Ocpp16::CustomOperation("FirmwareStatusNotification",
                    [ &checkProcessed] (JsonObject payload) {
                        //process req
                        if (checkProcessed == 0) {
                            REQUIRE( !strcmp(payload["status"] | "_Undefined", "Installing") );
                            checkProcessed++;
                        } else if (checkProcessed == 1) {
                            REQUIRE( !strcmp(payload["status"] | "_Undefined", "Installed") );
                            checkProcessed++;
                        }
                    },
                    [] () {
                        //create conf
                        return createEmptyDocument();
                    });
            });
        
        bool checkProcessedOnInstall = false;

        fwService->setOnInstall([&checkProcessedOnInstall] (const char *location) {
            checkProcessedOnInstall = true;
            REQUIRE( !strcmp(location, FTP_URL) );
            return true;
        });

        int checkProcessedOnInstallStatus = 0;

        fwService->setInstallationStatusInput([&checkProcessed, &checkProcessedOnInstallStatus] () {
            if (checkProcessed == 0) {
                if (checkProcessedOnInstallStatus == 0) checkProcessedOnInstallStatus = 1;
                return InstallationStatus::NotInstalled;
            } else {
                if (checkProcessedOnInstallStatus == 1) checkProcessedOnInstallStatus = 2;
                return InstallationStatus::Installed;
            }
        });
        
        getOcppContext()->initiateRequest(makeRequest(new Ocpp16::CustomOperation(
                "UpdateFirmware",
                [] () {
                    //create req
                    auto doc = makeJsonDoc("UnitTests", JSON_OBJECT_SIZE(4));
                    auto payload = doc->to<JsonObject>();
                    payload["location"] = FTP_URL;
                    payload["retries"] = 1;
                    payload["retrieveDate"] = BASE_TIME;
                    payload["retryInterval"] = 1;
                    return doc;},
                [] (JsonObject) { } //ignore conf
        )));

        for (unsigned int i = 0; i < 10; i++) {
            loop();
            mtime += 5000;
        }

        REQUIRE( checkProcessed == 2 ); //all FirmwareStatusNotification messages have been received
        REQUIRE( checkProcessedOnInstall );
        REQUIRE( checkProcessedOnInstallStatus == 2 );
    }

    SECTION("Download and install") {

        int checkProcessed = 0;

        getOcppContext()->getOperationRegistry().registerOperation("FirmwareStatusNotification",
            [&checkProcessed] () {
                return new Ocpp16::CustomOperation("FirmwareStatusNotification",
                    [ &checkProcessed] (JsonObject payload) {
                        //process req
                        if (checkProcessed == 0) {
                            REQUIRE( !strcmp(payload["status"] | "_Undefined", "Downloading") );
                            checkProcessed++;
                        } else if (checkProcessed == 1) {
                            REQUIRE( !strcmp(payload["status"] | "_Undefined", "Downloaded") );
                            checkProcessed++;
                        } else if (checkProcessed == 2) {
                            REQUIRE( !strcmp(payload["status"] | "_Undefined", "Installing") );
                            checkProcessed++;
                        } else if (checkProcessed == 3) {
                            REQUIRE( !strcmp(payload["status"] | "_Undefined", "Installed") );
                            checkProcessed++;
                        }
                    },
                    [] () {
                        //create conf
                        return createEmptyDocument();
                    });
            });
        
        bool checkProcessedOnDownload = false;
        fwService->setOnDownload([&checkProcessedOnDownload] (const char *location) {
            checkProcessedOnDownload = true;
            return true;
        });

        int checkProcessedOnDownloadStatus = 0;
        fwService->setDownloadStatusInput([&checkProcessed, &checkProcessedOnDownloadStatus] () {
            if (checkProcessed == 0) {
                if (checkProcessedOnDownloadStatus == 0) checkProcessedOnDownloadStatus = 1;
                return DownloadStatus::NotDownloaded;
            } else {
                if (checkProcessedOnDownloadStatus == 1) checkProcessedOnDownloadStatus = 2;
                return DownloadStatus::Downloaded;
            }
        });
        
        bool checkProcessedOnInstall = false;
        fwService->setOnInstall([&checkProcessedOnInstall] (const char *location) {
            checkProcessedOnInstall = true;
            return true;
        });

        int checkProcessedOnInstallStatus = 0;
        fwService->setInstallationStatusInput([&checkProcessed, &checkProcessedOnInstallStatus] () {
            if (checkProcessed <= 2) {
                if (checkProcessedOnInstallStatus == 0) checkProcessedOnInstallStatus = 1;
                return InstallationStatus::NotInstalled;
            } else {
                if (checkProcessedOnInstallStatus == 1) checkProcessedOnInstallStatus = 2;
                return InstallationStatus::Installed;
            }
        });
        
        getOcppContext()->initiateRequest(makeRequest(new Ocpp16::CustomOperation(
                "UpdateFirmware",
                [] () {
                    //create req
                    auto doc = makeJsonDoc("UnitTests", JSON_OBJECT_SIZE(4));
                    auto payload = doc->to<JsonObject>();
                    payload["location"] = FTP_URL;
                    payload["retries"] = 1;
                    payload["retrieveDate"] = BASE_TIME;
                    payload["retryInterval"] = 1;
                    return doc;},
                [] (JsonObject) { } //ignore conf
        )));

        for (unsigned int i = 0; i < 10; i++) {
            loop();
            mtime += 5000;
        }

        REQUIRE( checkProcessed == 4 ); //all FirmwareStatusNotification messages have been received
        REQUIRE( checkProcessedOnDownload );
        REQUIRE( checkProcessedOnDownloadStatus == 2 );
        REQUIRE( checkProcessedOnInstall );
        REQUIRE( checkProcessedOnInstallStatus == 2 );
    }

    SECTION("Download failure (try 2 times)") {

        int checkProcessed = 0;

        getOcppContext()->getOperationRegistry().registerOperation("FirmwareStatusNotification",
            [&checkProcessed] () {
                return new Ocpp16::CustomOperation("FirmwareStatusNotification",
                    [ &checkProcessed] (JsonObject payload) {
                        //process req
                        if (checkProcessed == 0) {
                            REQUIRE( !strcmp(payload["status"] | "_Undefined", "Downloading") );
                            checkProcessed++;
                        } else if (checkProcessed == 1) {
                            REQUIRE( !strcmp(payload["status"] | "_Undefined", "DownloadFailed") );
                            checkProcessed++;
                        } else if (checkProcessed == 2) {
                            REQUIRE( !strcmp(payload["status"] | "_Undefined", "Downloading") );
                            checkProcessed++;
                        } else if (checkProcessed == 3) {
                            REQUIRE( !strcmp(payload["status"] | "_Undefined", "DownloadFailed") );
                            checkProcessed++;
                        }
                    },
                    [] () {
                        //create conf
                        return createEmptyDocument();
                    });
            });

        int checkProcessedOnDownload = 0;
        fwService->setOnDownload([&checkProcessedOnDownload] (const char *location) {
            checkProcessedOnDownload++;
            return true;
        });

        int checkProcessedOnDownloadStatus = 0;
        fwService->setDownloadStatusInput([&checkProcessed, &checkProcessedOnDownloadStatus] () {
            if (checkProcessed == 0) {
                if (checkProcessedOnDownloadStatus == 0) checkProcessedOnDownloadStatus = 1;
                return DownloadStatus::NotDownloaded;
            } else if (checkProcessed == 1) {
                if (checkProcessedOnDownloadStatus == 1) checkProcessedOnDownloadStatus = 2;
                return DownloadStatus::DownloadFailed;
            } else if (checkProcessed == 2) {
                if (checkProcessedOnDownloadStatus == 2) checkProcessedOnDownloadStatus = 3;
                return DownloadStatus::NotDownloaded;
            } else {
                if (checkProcessedOnDownloadStatus == 3) checkProcessedOnDownloadStatus = 4;
                return DownloadStatus::DownloadFailed;
            }
        });

        bool checkProcessedOnInstall = false; // must not be executed
        fwService->setOnInstall([&checkProcessedOnInstall] (const char *location) {
            checkProcessedOnInstall = true;
            return true;
        });

        getOcppContext()->initiateRequest(makeRequest(new Ocpp16::CustomOperation(
                "UpdateFirmware",
                [] () {
                    //create req
                    auto doc = makeJsonDoc("UnitTests", JSON_OBJECT_SIZE(4));
                    auto payload = doc->to<JsonObject>();
                    payload["location"] = FTP_URL;
                    payload["retries"] = 2;
                    payload["retrieveDate"] = BASE_TIME;
                    payload["retryInterval"] = 10;
                    return doc;},
                [] (JsonObject) { } //ignore conf
        )));

        for (unsigned int i = 0; i < 20; i++) {
            loop();
            mtime += 5000;
        }

        REQUIRE( checkProcessed == 4 ); //all FirmwareStatusNotification messages have been received
        REQUIRE( checkProcessedOnDownload == 2 );
        REQUIRE( checkProcessedOnDownloadStatus == 4 );
        REQUIRE( !checkProcessedOnInstall );
    }

    SECTION("Installation failure (try 2 times)") {

        int checkProcessed = 0;

        getOcppContext()->getOperationRegistry().registerOperation("FirmwareStatusNotification",
            [&checkProcessed] () {
                return new Ocpp16::CustomOperation("FirmwareStatusNotification",
                    [ &checkProcessed] (JsonObject payload) {
                        //process req
                        if (checkProcessed == 0) {
                            REQUIRE( !strcmp(payload["status"] | "_Undefined", "Installing") );
                            checkProcessed++;
                        } else if (checkProcessed == 1) {
                            REQUIRE( !strcmp(payload["status"] | "_Undefined", "InstallationFailed") );
                            checkProcessed++;
                        } else if (checkProcessed == 2) {
                            REQUIRE( !strcmp(payload["status"] | "_Undefined", "Installing") );
                            checkProcessed++;
                        } else if (checkProcessed == 3) {
                            REQUIRE( !strcmp(payload["status"] | "_Undefined", "InstallationFailed") );
                            checkProcessed++;
                        }
                    },
                    [] () {
                        //create conf
                        return createEmptyDocument();
                    });
            });
        
        int checkProcessedOnInstall = 0;

        fwService->setOnInstall([&checkProcessedOnInstall] (const char *location) {
            checkProcessedOnInstall++;
            return true;
        });

        int checkProcessedOnInstallStatus = 0;

        fwService->setInstallationStatusInput([&checkProcessed, &checkProcessedOnInstallStatus] () {
            if (checkProcessed == 0) {
                if (checkProcessedOnInstallStatus == 0) checkProcessedOnInstallStatus = 1;
                return InstallationStatus::NotInstalled;
            } else if (checkProcessed == 1) {
                if (checkProcessedOnInstallStatus == 1) checkProcessedOnInstallStatus = 2;
                return InstallationStatus::InstallationFailed;
            } else if (checkProcessed == 2) {
                if (checkProcessedOnInstallStatus == 2) checkProcessedOnInstallStatus = 3;
                return InstallationStatus::NotInstalled;
            } else {
                if (checkProcessedOnInstallStatus == 3) checkProcessedOnInstallStatus = 4;
                return InstallationStatus::InstallationFailed;
            }
        });
        
        getOcppContext()->initiateRequest(makeRequest(new Ocpp16::CustomOperation(
                "UpdateFirmware",
                [] () {
                    //create req
                    auto doc = makeJsonDoc("UnitTests", JSON_OBJECT_SIZE(4));
                    auto payload = doc->to<JsonObject>();
                    payload["location"] = FTP_URL;
                    payload["retries"] = 2;
                    payload["retrieveDate"] = BASE_TIME;
                    payload["retryInterval"] = 10;
                    return doc;},
                [] (JsonObject) { } //ignore conf
        )));

        for (unsigned int i = 0; i < 10; i++) {
            loop();
            mtime += 5000;
        }

        REQUIRE( checkProcessed == 4 ); //all FirmwareStatusNotification messages have been received
        REQUIRE( checkProcessedOnInstall == 2 );
        REQUIRE( checkProcessedOnInstallStatus == 4 );
    }

    SECTION("Wait for retreiveDate and charging sessions end") {

        beginTransaction("mIdTag");

        int checkProcessed = 0;

        getOcppContext()->getOperationRegistry().registerOperation("FirmwareStatusNotification",
            [&checkProcessed] () {
                return new Ocpp16::CustomOperation("FirmwareStatusNotification",
                    [ &checkProcessed] (JsonObject payload) {
                        //process req
                        if (checkProcessed == 0) {
                            REQUIRE( !strcmp(payload["status"] | "_Undefined", "Downloading") );
                            checkProcessed++;
                        } else if (checkProcessed == 1) {
                            REQUIRE( !strcmp(payload["status"] | "_Undefined", "Downloaded") );
                            checkProcessed++;
                        } else if (checkProcessed == 2) {
                            REQUIRE( !strcmp(payload["status"] | "_Undefined", "Installing") );
                            checkProcessed++;
                        } else if (checkProcessed == 3) {
                            REQUIRE( !strcmp(payload["status"] | "_Undefined", "Installed") );
                            checkProcessed++;
                        }
                    },
                    [] () {
                        //create conf
                        return createEmptyDocument();
                    });
            });
        
        bool checkProcessedOnDownload = false;
        fwService->setOnDownload([&checkProcessedOnDownload] (const char *location) {
            checkProcessedOnDownload = true;
            return true;
        });

        int checkProcessedOnDownloadStatus = 0;
        fwService->setDownloadStatusInput([&checkProcessed, &checkProcessedOnDownloadStatus] () {
            if (checkProcessed == 0) {
                if (checkProcessedOnDownloadStatus == 0) checkProcessedOnDownloadStatus = 1;
                return DownloadStatus::NotDownloaded;
            } else {
                if (checkProcessedOnDownloadStatus == 1) checkProcessedOnDownloadStatus = 2;
                return DownloadStatus::Downloaded;
            }
        });
        
        bool checkProcessedOnInstall = false;
        fwService->setOnInstall([&checkProcessedOnInstall] (const char *location) {
            checkProcessedOnInstall = true;
            return true;
        });

        int checkProcessedOnInstallStatus = 0;
        fwService->setInstallationStatusInput([&checkProcessed, &checkProcessedOnInstallStatus] () {
            if (checkProcessed <= 2) {
                if (checkProcessedOnInstallStatus == 0) checkProcessedOnInstallStatus = 1;
                return InstallationStatus::NotInstalled;
            } else {
                if (checkProcessedOnInstallStatus == 1) checkProcessedOnInstallStatus = 2;
                return InstallationStatus::Installed;
            }
        });
        
        getOcppContext()->initiateRequest(makeRequest(new Ocpp16::CustomOperation(
                "UpdateFirmware",
                [] () {
                    //create req
                    auto doc = makeJsonDoc("UnitTests", JSON_OBJECT_SIZE(4));
                    auto payload = doc->to<JsonObject>();
                    payload["location"] = FTP_URL;
                    payload["retries"] = 1;
                    payload["retrieveDate"] = BASE_TIME_1H;
                    payload["retryInterval"] = 1;
                    return doc;},
                [] (JsonObject) { } //ignore conf
        )));

        for (unsigned int i = 0; i < 10; i++) {
            loop();
            mtime += 5000;
        }

        //retreiveDate not reached yet
        REQUIRE( checkProcessed == 0 );
        REQUIRE( !checkProcessedOnDownload );
        REQUIRE( checkProcessedOnDownloadStatus == 0 );
        REQUIRE( !checkProcessedOnInstall );
        REQUIRE( checkProcessedOnInstallStatus == 0 );

        getOcppContext()->getModel().getClock().setTime(BASE_TIME_1H);

        for (unsigned int i = 0; i < 10; i++) {
            loop();
            mtime += 5000;
        }

        //download-related FirmwareStatusNotification messages have been received
        REQUIRE( checkProcessed == 2 );
        REQUIRE( checkProcessedOnDownload );
        REQUIRE( checkProcessedOnDownloadStatus == 2 );
        REQUIRE( !checkProcessedOnInstall );
        REQUIRE( checkProcessedOnInstallStatus == 0 );

        endTransaction();

        for (unsigned int i = 0; i < 10; i++) {
            loop();
            mtime += 5000;
        }

        //all FirmwareStatusNotification messages have been received
        REQUIRE( checkProcessed == 4 );
        REQUIRE( checkProcessedOnDownload );
        REQUIRE( checkProcessedOnDownloadStatus == 2 );
        REQUIRE( checkProcessedOnInstall );
        REQUIRE( checkProcessedOnInstallStatus == 2 );
    }

    mocpp_deinitialize();

}
