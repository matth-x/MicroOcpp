// matth-x/MicroOcpp
// Copyright Matthias Akstaller 2019 - 2024
// MIT License

#include "MicroOcpp.h"

#include <MicroOcpp/Core/Context.h>
#include <MicroOcpp/Model/Model.h>
#include <MicroOcpp/Model/Metering/MeteringService.h>
#include <MicroOcpp/Model/SmartCharging/SmartChargingService.h>
#include <MicroOcpp/Model/ConnectorBase/ConnectorsCommon.h>
#include <MicroOcpp/Model/Heartbeat/HeartbeatService.h>
#include <MicroOcpp/Model/FirmwareManagement/FirmwareService.h>
#include <MicroOcpp/Model/Diagnostics/DiagnosticsService.h>
#include <MicroOcpp/Model/Transactions/TransactionStore.h>
#include <MicroOcpp/Model/Authorization/AuthorizationService.h>
#include <MicroOcpp/Model/Reservation/ReservationService.h>
#include <MicroOcpp/Model/Boot/BootService.h>
#include <MicroOcpp/Model/Reset/ResetService.h>
#include <MicroOcpp/Model/Variables/VariableService.h>
#include <MicroOcpp/Model/Transactions/TransactionService.h>
#include <MicroOcpp/Model/Certificates/CertificateService.h>
#include <MicroOcpp/Model/Certificates/CertificateMbedTLS.h>
#include <MicroOcpp/Model/Availability/AvailabilityService.h>
#include <MicroOcpp/Model/RemoteControl/RemoteControlService.h>
#include <MicroOcpp/Core/Request.h>
#include <MicroOcpp/Core/OperationRegistry.h>
#include <MicroOcpp/Core/FilesystemAdapter.h>
#include <MicroOcpp/Core/FilesystemUtils.h>
#include <MicroOcpp/Core/Ftp.h>
#include <MicroOcpp/Core/FtpMbedTLS.h>

#include <MicroOcpp/Operations/Authorize.h>
#include <MicroOcpp/Operations/StartTransaction.h>
#include <MicroOcpp/Operations/StopTransaction.h>
#include <MicroOcpp/Operations/CustomOperation.h>

#include <MicroOcpp/Debug.h>

namespace MicroOcpp {
namespace Facade {

#ifndef MO_CUSTOM_WS
WebSocketsClient *webSocket {nullptr};
Connection *connection {nullptr};
#endif

Context *context {nullptr};
std::shared_ptr<FilesystemAdapter> filesystem;

#ifndef MO_NUMCONNECTORS
#define MO_NUMCONNECTORS 2
#endif

#define OCPP_ID_OF_CP 0
#define OCPP_ID_OF_CONNECTOR 1

} //end namespace MicroOcpp::Facade
} //end namespace MicroOcpp

#if MO_ENABLE_HEAP_PROFILER
#ifndef MO_HEAP_PROFILER_EXTERNAL_CONTROL
#define MO_HEAP_PROFILER_EXTERNAL_CONTROL 0 //enable if you want to manually reset the heap profiler (e.g. for keeping stats over multiple MO lifecycles)
#endif
#endif

using namespace MicroOcpp;
using namespace MicroOcpp::Facade;
using namespace MicroOcpp::Ocpp16;

#ifndef MO_CUSTOM_WS
void mocpp_initialize(const char *backendUrl, const char *chargeBoxId, const char *chargePointModel, const char *chargePointVendor, FilesystemOpt fsOpt, const char *password, const char *CA_cert, bool autoRecover) {
    if (context) {
        MO_DBG_WARN("already initialized. To reinit, call mocpp_deinitialize() before");
        return;
    }

    if (!backendUrl || !chargePointModel || !chargePointVendor) {
        MO_DBG_ERR("invalid args");
        return;
    }

    if (!chargeBoxId) {
        chargeBoxId = "";
    }

    /*
     * parse backendUrl so that it suits the links2004/arduinoWebSockets interface
     */
    auto url = makeString("MicroOcpp.cpp", backendUrl);

    //tolower protocol specifier
    for (auto c = url.begin(); *c != ':' && c != url.end(); c++) {
        *c = tolower(*c);
    }

    bool isTLS = true;
    if (!strncmp(url.c_str(),"wss://",strlen("wss://"))) {
        isTLS = true;
    } else if (!strncmp(url.c_str(),"ws://",strlen("ws://"))) {
        isTLS = false;
    } else {
        MO_DBG_ERR("only ws:// and wss:// supported");
        return;
    }

    //parse host, port
    auto host_port_path = url.substr(url.find_first_of("://") + strlen("://"));
    auto host_port = host_port_path.substr(0, host_port_path.find_first_of('/'));
    auto path = host_port_path.substr(host_port.length());
    auto host = host_port.substr(0, host_port.find_first_of(':'));
    if (host.empty()) {
        MO_DBG_ERR("could not parse host: %s", url.c_str());
        return;
    }
    uint16_t port = 0;
    auto port_str = host_port.substr(host.length());
    if (port_str.empty()) {
        port = isTLS ? 443U : 80U;
    } else {
        //skip leading ':'
        port_str = port_str.substr(1);
        for (auto c = port_str.begin(); c != port_str.end(); c++) {
            if (*c < '0' || *c > '9') {
                MO_DBG_ERR("could not parse port: %s", url.c_str());
                return; 
            }
            auto p = port * 10U + (*c - '0');
            if (p < port) {
                MO_DBG_ERR("could not parse port (overflow): %s", url.c_str());
                return;
            }
            port = p;
        }
    }

    if (path.empty()) {
        path = "/";
    }

    if ((!*chargeBoxId) == '\0') {
        if (path.back() != '/') {
            path += '/';
        }

        path += chargeBoxId;
    }

    MO_DBG_INFO("connecting to %s -- (host: %s, port: %u, path: %s)", url.c_str(), host.c_str(), port, path.c_str());

    if (!webSocket)
        webSocket = new WebSocketsClient();

    if (isTLS) {
        // server address, port, path and TLS certificate
        webSocket->beginSslWithCA(host.c_str(), port, path.c_str(), CA_cert, "ocpp1.6");
    } else {
        // server address, port, path
        webSocket->begin(host.c_str(), port, path.c_str(), "ocpp1.6");
    }

    // try ever 5000 again if connection has failed
    webSocket->setReconnectInterval(5000);

    // start heartbeat (optional)
    // ping server every 15000 ms
    // expect pong from server within 3000 ms
    // consider connection disconnected if pong is not received 2 times
    webSocket->enableHeartbeat(15000, 3000, 2); //comment this one out to for specific OCPP servers

    // add authentication data (optional)
    if (password && strlen(password) + strlen(chargeBoxId) >= 4) {
        webSocket->setAuthorization(chargeBoxId, password);
    }

    delete connection;
    connection = new EspWiFi::WSClient(webSocket);

    mocpp_initialize(*connection, ChargerCredentials(chargePointModel, chargePointVendor), makeDefaultFilesystemAdapter(fsOpt), autoRecover);
}
#endif

ChargerCredentials::ChargerCredentials(const char *cpModel, const char *cpVendor, const char *fWv, const char *cpSNr, const char *meterSNr, const char *meterType, const char *cbSNr, const char *iccid, const char *imsi) {

    StaticJsonDocument<512> creds;
    if (cbSNr)
        creds["chargeBoxSerialNumber"] = cbSNr;
    if (cpModel)
        creds["chargePointModel"] = cpModel;
    if (cpSNr)
        creds["chargePointSerialNumber"] = cpSNr;
    if (cpVendor)
        creds["chargePointVendor"] = cpVendor;
    if (fWv)
        creds["firmwareVersion"] = fWv;
    if (iccid)
        creds["iccid"] = iccid;
    if (imsi)
        creds["imsi"] = imsi;
    if (meterSNr)
        creds["meterSerialNumber"] = meterSNr;
    if (meterType)
        creds["meterType"] = meterType;
    
    if (creds.overflowed()) {
        MO_DBG_ERR("Charger Credentials too long");
    }

    size_t written = serializeJson(creds, payload, 512);

    if (written < 2) {
        MO_DBG_ERR("Charger Credentials could not be written");
        sprintf(payload, "{}");
    }
}

ChargerCredentials ChargerCredentials::v201(const char *cpModel, const char *cpVendor, const char *fWv, const char *cpSNr, const char *meterSNr, const char *meterType, const char *cbSNr, const char *iccid, const char *imsi) {

    ChargerCredentials res;

    StaticJsonDocument<512> creds;
    if (cpSNr)
        creds["serialNumber"] = cpSNr;
    if (cpModel)
        creds["model"] = cpModel;
    if (cpVendor)
        creds["vendorName"] = cpVendor;
    if (fWv)
        creds["firmwareVersion"] = fWv;
    if (iccid)
        creds["modem"]["iccid"] = iccid;
    if (imsi)
        creds["modem"]["imsi"] = imsi;

    if (creds.overflowed()) {
        MO_DBG_ERR("Charger Credentials too long");
    }

    size_t written = serializeJson(creds, res.payload, 512);

    if (written < 2) {
        MO_DBG_ERR("Charger Credentials could not be written");
        sprintf(res.payload, "{}");
    }

    return res;
}

void mocpp_initialize(Connection& connection, const char *bootNotificationCredentials, std::shared_ptr<FilesystemAdapter> fs, bool autoRecover, MicroOcpp::ProtocolVersion version) {
    if (context) {
        MO_DBG_WARN("already initialized. To reinit, call mocpp_deinitialize() before");
        return;
    }

    MO_DBG_DEBUG("initialize OCPP");

    filesystem = fs;
    MO_DBG_DEBUG("filesystem %s", filesystem ? "loaded" : "deactivated");

    BootStats bootstats;
    BootService::loadBootStats(filesystem, bootstats);

    if (autoRecover && bootstats.getBootFailureCount() > 3) {
        BootService::recover(filesystem, bootstats);
        bootstats = BootStats();
    }

    BootService::migrate(filesystem, bootstats);

    bootstats.bootNr++; //assign new boot number to this run
    BootService::storeBootStats(filesystem, bootstats);

    configuration_init(filesystem); //call before each other library call

    context = new Context(connection, filesystem, bootstats.bootNr, version);

#if MO_ENABLE_MBEDTLS
    context->setFtpClient(makeFtpClientMbedTLS());
#endif //MO_ENABLE_MBEDTLS

    auto& model = context->getModel();

    model.setBootService(std::unique_ptr<BootService>(
        new BootService(*context, filesystem)));

#if MO_ENABLE_V201
    if (version.major == 2) {
        model.setAvailabilityService(std::unique_ptr<AvailabilityService>(
            new AvailabilityService(*context, MO_NUM_EVSEID)));
        model.setVariableService(std::unique_ptr<VariableService>(
            new VariableService(*context, filesystem)));
        model.setTransactionService(std::unique_ptr<TransactionService>(
            new TransactionService(*context, filesystem, MO_NUM_EVSEID)));
        model.setRemoteControlService(std::unique_ptr<RemoteControlService>(
            new RemoteControlService(*context, MO_NUM_EVSEID)));
    } else
#endif
    {
        model.setTransactionStore(std::unique_ptr<TransactionStore>(
            new TransactionStore(MO_NUMCONNECTORS, filesystem)));
        model.setConnectorsCommon(std::unique_ptr<ConnectorsCommon>(
            new ConnectorsCommon(*context, MO_NUMCONNECTORS, filesystem)));
        auto connectors = makeVector<std::unique_ptr<Connector>>("v16.ConnectorBase.Connector");
        for (unsigned int connectorId = 0; connectorId < MO_NUMCONNECTORS; connectorId++) {
            connectors.emplace_back(new Connector(*context, filesystem, connectorId));
        }
        model.setConnectors(std::move(connectors));
    }
    model.setHeartbeatService(std::unique_ptr<HeartbeatService>(
        new HeartbeatService(*context)));

#if MO_ENABLE_LOCAL_AUTH
    model.setAuthorizationService(std::unique_ptr<AuthorizationService>(
        new AuthorizationService(*context, filesystem)));
#endif //MO_ENABLE_LOCAL_AUTH

#if MO_ENABLE_RESERVATION
    model.setReservationService(std::unique_ptr<ReservationService>(
        new ReservationService(*context, MO_NUMCONNECTORS)));
#endif

#if MO_ENABLE_CERT_MGMT && MO_ENABLE_CERT_STORE_MBEDTLS
    std::unique_ptr<CertificateStore> certStore = makeCertificateStoreMbedTLS(filesystem);
    if (certStore) {
        model.setCertificateService(std::unique_ptr<CertificateService>(
            new CertificateService(*context)));
    }
    if (certStore && model.getCertificateService()) {
        model.getCertificateService()->setCertificateStore(std::move(certStore));
    }
#endif

#if MO_ENABLE_V201
    if (version.major == 2) {
        //depends on VariableService
        model.setResetServiceV201(std::unique_ptr<Ocpp201::ResetService>(
            new Ocpp201::ResetService(*context)));
    } else
#endif
    {
        model.setResetService(std::unique_ptr<ResetService>(
            new ResetService(*context)));
    }

#if !defined(MO_CUSTOM_UPDATER)
#if MO_PLATFORM == MO_PLATFORM_ARDUINO && defined(ESP32) && MO_ENABLE_MBEDTLS
    model.setFirmwareService(
        makeDefaultFirmwareService(*context)); //instantiate FW service + ESP installation routine
#elif MO_PLATFORM == MO_PLATFORM_ARDUINO && defined(ESP8266)
    model.setFirmwareService(
        makeDefaultFirmwareService(*context)); //instantiate FW service + ESP installation routine
#endif //MO_PLATFORM
#endif //!defined(MO_CUSTOM_UPDATER)

#if !defined(MO_CUSTOM_DIAGNOSTICS)
#if MO_PLATFORM == MO_PLATFORM_ARDUINO && defined(ESP32) && MO_ENABLE_MBEDTLS
    model.setDiagnosticsService(
        makeDefaultDiagnosticsService(*context, filesystem)); //instantiate Diag service + ESP hardware diagnostics
#elif MO_ENABLE_MBEDTLS
    model.setDiagnosticsService(
        makeDefaultDiagnosticsService(*context, filesystem)); //instantiate Diag service 
#endif //MO_PLATFORM
#endif //!defined(MO_CUSTOM_DIAGNOSTICS)

#if MO_PLATFORM == MO_PLATFORM_ARDUINO && (defined(ESP32) || defined(ESP8266))
    setOnResetExecute(makeDefaultResetFn());
#endif

    model.getBootService()->setChargePointCredentials(bootNotificationCredentials);

    auto credsJson = model.getBootService()->getChargePointCredentials();
    if (model.getFirmwareService() && credsJson && credsJson->containsKey("firmwareVersion")) {
        model.getFirmwareService()->setBuildNumber((*credsJson)["firmwareVersion"]);
    }
    credsJson.reset();

    configuration_load();

    MO_DBG_INFO("initialized MicroOcpp v" MO_VERSION " running OCPP %i.%i.%i", version.major, version.minor, version.patch);
}

void mocpp_deinitialize() {

    if (context) {
        //release bootstats recovery mechanism
        BootStats bootstats;
        BootService::loadBootStats(filesystem, bootstats);
        if (bootstats.lastBootSuccess != bootstats.bootNr) {
            MO_DBG_DEBUG("boot success timer override");
            bootstats.lastBootSuccess = bootstats.bootNr;
            BootService::storeBootStats(filesystem, bootstats);
        }
    }
    
    delete context;
    context = nullptr;

#ifndef MO_CUSTOM_WS
    delete connection;
    connection = nullptr;
    delete webSocket;
    webSocket = nullptr;
#endif

    filesystem.reset();

    configuration_deinit();

#if !MO_HEAP_PROFILER_EXTERNAL_CONTROL
    MO_MEM_DEINIT();
#endif

    MO_DBG_DEBUG("deinitialized OCPP\n");
}

void mocpp_loop() {
    if (!context) {
        MO_DBG_WARN("need to call mocpp_initialize before");
        return;
    }

    context->loop();
}

std::shared_ptr<Transaction> beginTransaction(const char *idTag, unsigned int connectorId) {
    if (!context) {
        MO_DBG_ERR("OCPP uninitialized"); //need to call mocpp_initialize before
        return nullptr;
    }
    if (!idTag || strnlen(idTag, IDTAG_LEN_MAX + 2) > IDTAG_LEN_MAX) {
        MO_DBG_ERR("idTag format violation. Expect c-style string with at most %u characters", IDTAG_LEN_MAX);
        return nullptr;
    }
    auto connector = context->getModel().getConnector(connectorId);
    if (!connector) {
        MO_DBG_ERR("could not find connector");
        return nullptr;
    }

    return connector->beginTransaction(idTag);
}

std::shared_ptr<Transaction> beginTransaction_authorized(const char *idTag, const char *parentIdTag, unsigned int connectorId) {
    if (!context) {
        MO_DBG_ERR("OCPP uninitialized"); //need to call mocpp_initialize before
        return nullptr;
    }
    if (!idTag || strnlen(idTag, IDTAG_LEN_MAX + 2) > IDTAG_LEN_MAX ||
        (parentIdTag && strnlen(parentIdTag, IDTAG_LEN_MAX + 2) > IDTAG_LEN_MAX)) {
        MO_DBG_ERR("(parent)idTag format violation. Expect c-style string with at most %u characters", IDTAG_LEN_MAX);
        return nullptr;
    }
    auto connector = context->getModel().getConnector(connectorId);
    if (!connector) {
        MO_DBG_ERR("could not find connector");
        return nullptr;
    }
    
    return connector->beginTransaction_authorized(idTag, parentIdTag);
}

bool endTransaction(const char *idTag, const char *reason, unsigned int connectorId) {
    if (!context) {
        MO_DBG_ERR("OCPP uninitialized"); //need to call mocpp_initialize before
        return false;
    }
    bool res = false;
    if (isTransactionActive(connectorId) && getTransactionIdTag(connectorId)) {
        //end transaction now if either idTag is nullptr (i.e. force stop) or the idTag matches beginTransaction
        if (!idTag || !strcmp(idTag, getTransactionIdTag(connectorId))) {
            res = endTransaction_authorized(idTag, reason, connectorId);
        } else {
            auto tx = getTransaction(connectorId);
            const char *parentIdTag = tx->getParentIdTag();
            if (strlen(parentIdTag) > 0)
            {
                // We have a parent ID tag, so we need to check if this new card also has one
                auto authorize = makeRequest(new Ocpp16::Authorize(context->getModel(), idTag));
                auto idTag_capture = makeString("MicroOcpp.cpp", idTag);
                auto reason_capture = makeString("MicroOcpp.cpp", reason ? reason : "");
                authorize->setOnReceiveConfListener([idTag_capture, reason_capture, connectorId, tx] (JsonObject response) {
                    JsonObject idTagInfo = response["idTagInfo"];

                    if (strcmp("Accepted", idTagInfo["status"] | "UNDEFINED")) {
                        //Authorization rejected, do nothing
                        MO_DBG_DEBUG("Authorize rejected (%s), continue transaction", idTag_capture.c_str());
                        auto connector = context->getModel().getConnector(connectorId);
                        if (connector) {
                            connector->updateTxNotification(TxNotification::AuthorizationRejected);
                        }
                        return;
                    }
                    if (idTagInfo.containsKey("parentIdTag") && !strcmp(idTagInfo["parenIdTag"], tx->getParentIdTag()))
                    {
                        endTransaction_authorized(idTag_capture.c_str(), reason_capture.empty() ? (const char*)nullptr : reason_capture.c_str(), connectorId);
                    }
                });

                authorize->setOnTimeoutListener([idTag_capture, connectorId] () {
                    //Authorization timed out, do nothing
                    MO_DBG_DEBUG("Authorization timeout (%s), continue transaction", idTag_capture.c_str());
                    auto connector = context->getModel().getConnector(connectorId);
                    if (connector) {
                        connector->updateTxNotification(TxNotification::AuthorizationTimeout);
                    }
                });

                auto authorizationTimeoutInt = declareConfiguration<int>(MO_CONFIG_EXT_PREFIX "AuthorizationTimeout", 20);
                authorize->setTimeout(authorizationTimeoutInt && authorizationTimeoutInt->getInt() > 0 ? authorizationTimeoutInt->getInt() * 1000UL : 20UL * 1000UL);

                context->initiateRequest(std::move(authorize));
                res = true;
            } else {
                MO_DBG_INFO("endTransaction: idTag doesn't match");
                (void)0;
            }
        }
    }
    return res;
}

bool endTransaction_authorized(const char *idTag, const char *reason, unsigned int connectorId) {
    if (!context) {
        MO_DBG_ERR("OCPP uninitialized"); //need to call mocpp_initialize before
        return false;
    }
    auto connector = context->getModel().getConnector(connectorId);
    if (!connector) {
        MO_DBG_ERR("could not find connector");
        return false;
    }
    auto res = isTransactionActive(connectorId);
    connector->endTransaction(idTag, reason);
    return res;
}

bool isTransactionActive(unsigned int connectorId) {
    if (!context) {
        MO_DBG_ERR("OCPP uninitialized"); //need to call mocpp_initialize before
        return false;
    }

    #if MO_ENABLE_V201
    if (context->getVersion().major == 2) {
        TransactionService::Evse *evse = nullptr;
        if (auto txService = context->getModel().getTransactionService()) {
            evse = txService->getEvse(connectorId);
        }
        if (!evse) {
            MO_DBG_ERR("could not find EVSE");
            return false;
        }
        return evse->getTransaction() && evse->getTransaction()->active;
    }
    #endif

    auto connector = context->getModel().getConnector(connectorId);
    if (!connector) {
        MO_DBG_ERR("could not find connector");
        return false;
    }
    auto& tx = connector->getTransaction();
    return tx ? tx->isActive() : false;
}

bool isTransactionRunning(unsigned int connectorId) {
    if (!context) {
        MO_DBG_ERR("OCPP uninitialized"); //need to call mocpp_initialize before
        return false;
    }

    #if MO_ENABLE_V201
    if (context->getVersion().major == 2) {
        TransactionService::Evse *evse = nullptr;
        if (auto txService = context->getModel().getTransactionService()) {
            evse = txService->getEvse(connectorId);
        }
        if (!evse) {
            MO_DBG_ERR("could not find EVSE");
            return false;
        }
        return evse->getTransaction() && evse->getTransaction()->started && !evse->getTransaction()->stopped;
    }
    #endif

    auto connector = context->getModel().getConnector(connectorId);
    if (!connector) {
        MO_DBG_ERR("could not find connector");
        return false;
    }
    auto& tx = connector->getTransaction();
    return tx ? tx->isRunning() : false;
}

const char *getTransactionIdTag(unsigned int connectorId) {
    if (!context) {
        MO_DBG_ERR("OCPP uninitialized"); //need to call mocpp_initialize before
        return nullptr;
    }

    #if MO_ENABLE_V201
    if (context->getVersion().major == 2) {
        TransactionService::Evse *evse = nullptr;
        if (auto txService = context->getModel().getTransactionService()) {
            evse = txService->getEvse(connectorId);
        }
        if (!evse) {
            MO_DBG_ERR("could not find EVSE");
            return nullptr;
        }
        return evse->getTransaction() ? evse->getTransaction()->idToken.get() : nullptr;
    }
    #endif

    auto connector = context->getModel().getConnector(connectorId);
    if (!connector) {
        MO_DBG_ERR("could not find connector");
        return nullptr;
    }
    auto& tx = connector->getTransaction();
    return tx ? tx->getIdTag() : nullptr;
}

std::shared_ptr<Transaction> mocpp_undefinedTx;

std::shared_ptr<Transaction>& getTransaction(unsigned int connectorId) {
    if (!context) {
        MO_DBG_WARN("OCPP uninitialized");
        return mocpp_undefinedTx;
    }
    auto connector = context->getModel().getConnector(connectorId);
    if (!connector) {
        MO_DBG_ERR("could not find connector");
        return mocpp_undefinedTx;
    }
    return connector->getTransaction();
}

bool ocppPermitsCharge(unsigned int connectorId) {
    if (!context) {
        MO_DBG_WARN("OCPP uninitialized");
        return false;
    }
#if MO_ENABLE_V201
    if (context->getVersion().major == 2) {
        TransactionService::Evse *evse = nullptr;
        if (auto txService = context->getModel().getTransactionService()) {
            evse = txService->getEvse(connectorId);
        }
        if (!evse) {
            MO_DBG_ERR("could not find EVSE");
            return false;
        }
        return evse->ocppPermitsCharge();
    }
#endif
    auto connector = context->getModel().getConnector(connectorId);
    if (!connector) {
        MO_DBG_ERR("could not find connector");
        return false;
    }
    return connector->ocppPermitsCharge();
}

ChargePointStatus getChargePointStatus(unsigned int connectorId) {
    if (!context) {
        MO_DBG_WARN("OCPP uninitialized");
        return ChargePointStatus_UNDEFINED;
    }
#if MO_ENABLE_V201
    if (context->getVersion().major == 2) {
        if (auto availabilityService = context->getModel().getAvailabilityService()) {
            if (auto evse = availabilityService->getEvse(connectorId)) {
                return evse->getStatus();
            }
        }
    }
#endif
    auto connector = context->getModel().getConnector(connectorId);
    if (!connector) {
        MO_DBG_ERR("could not find connector");
        return ChargePointStatus_UNDEFINED;
    }
    return connector->getStatus();
}

void setConnectorPluggedInput(std::function<bool()> pluggedInput, unsigned int connectorId) {
    if (!context) {
        MO_DBG_ERR("OCPP uninitialized"); //need to call mocpp_initialize before
        return;
    }
#if MO_ENABLE_V201
    if (context->getVersion().major == 2) {
        if (auto availabilityService = context->getModel().getAvailabilityService()) {
            if (auto evse = availabilityService->getEvse(connectorId)) {
                evse->setConnectorPluggedInput(pluggedInput);
            }
        }
        if (auto txService = context->getModel().getTransactionService()) {
            if (auto evse = txService->getEvse(connectorId)) {
                evse->setConnectorPluggedInput(pluggedInput);
            }
        }
        return;
    }
#endif
    auto connector = context->getModel().getConnector(connectorId);
    if (!connector) {
        MO_DBG_ERR("could not find connector");
        return;
    }
    connector->setConnectorPluggedInput(pluggedInput);
}

void setEnergyMeterInput(std::function<int()> energyInput, unsigned int connectorId) {
    if (!context) {
        MO_DBG_ERR("OCPP uninitialized"); //need to call mocpp_initialize before
        return;
    }

    #if MO_ENABLE_V201
    if (context->getVersion().major == 2) {
        addMeterValueInput([energyInput] () {return static_cast<float>(energyInput());}, "Energy.Active.Import.Register", "Wh", nullptr, nullptr, connectorId);
        return;
    }
    #endif

    SampledValueProperties meterProperties;
    meterProperties.setMeasurand("Energy.Active.Import.Register");
    meterProperties.setUnit("Wh");
    auto mvs = std::unique_ptr<SampledValueSamplerConcrete<int32_t, SampledValueDeSerializer<int32_t>>>(
                           new SampledValueSamplerConcrete<int32_t, SampledValueDeSerializer<int32_t>>(
            meterProperties,
            [energyInput] (ReadingContext) {return energyInput();}
    ));
    addMeterValueInput(std::move(mvs), connectorId);
}

void setPowerMeterInput(std::function<float()> powerInput, unsigned int connectorId) {
    if (!context) {
        MO_DBG_ERR("OCPP uninitialized"); //need to call mocpp_initialize before
        return;
    }

    #if MO_ENABLE_V201
    if (context->getVersion().major == 2) {
        addMeterValueInput([powerInput] () {return static_cast<float>(powerInput());}, "Power.Active.Import", "W", nullptr, nullptr, connectorId);
        return;
    }
    #endif

    SampledValueProperties meterProperties;
    meterProperties.setMeasurand("Power.Active.Import");
    meterProperties.setUnit("W");
    auto mvs = std::unique_ptr<SampledValueSamplerConcrete<float, SampledValueDeSerializer<float>>>(
                           new SampledValueSamplerConcrete<float, SampledValueDeSerializer<float>>(
            meterProperties,
            [powerInput] (ReadingContext) {return powerInput();}
    ));
    addMeterValueInput(std::move(mvs), connectorId);
}

void setSmartChargingPowerOutput(std::function<void(float)> chargingLimitOutput, unsigned int connectorId) {
    if (!context) {
        MO_DBG_ERR("OCPP uninitialized"); //need to call mocpp_initialize before
        return;
    }
    if (!context->getModel().getConnector(connectorId)) {
        MO_DBG_ERR("could not find connector");
        return;
    }

    if (chargingLimitOutput) {
        setSmartChargingOutput([chargingLimitOutput] (float power, float current, int nphases) -> void {
            chargingLimitOutput(power);
        }, connectorId);
    } else {
        setSmartChargingOutput(nullptr, connectorId);
    }

    if (auto scService = context->getModel().getSmartChargingService()) {
        if (chargingLimitOutput) {
            scService->updateAllowedChargingRateUnit(true, false);
        } else {
            scService->updateAllowedChargingRateUnit(false, false);
        }
    }
}

void setSmartChargingCurrentOutput(std::function<void(float)> chargingLimitOutput, unsigned int connectorId) {
    if (!context) {
        MO_DBG_ERR("OCPP uninitialized"); //need to call mocpp_initialize before
        return;
    }
    if (!context->getModel().getConnector(connectorId)) {
        MO_DBG_ERR("could not find connector");
        return;
    }

    if (chargingLimitOutput) {
        setSmartChargingOutput([chargingLimitOutput] (float power, float current, int nphases) -> void {
            chargingLimitOutput(current);
        }, connectorId);
    } else {
        setSmartChargingOutput(nullptr, connectorId);
    }

    if (auto scService = context->getModel().getSmartChargingService()) {
        if (chargingLimitOutput) {
            scService->updateAllowedChargingRateUnit(false, true);
        } else {
            scService->updateAllowedChargingRateUnit(false, false);
        }
    }
}

void setSmartChargingOutput(std::function<void(float,float,int)> chargingLimitOutput, unsigned int connectorId) {
    if (!context) {
        MO_DBG_ERR("OCPP uninitialized"); //need to call mocpp_initialize before
        return;
    }
    if (!context->getModel().getConnector(connectorId)) {
        MO_DBG_ERR("could not find connector");
        return;
    }

    auto& model = context->getModel();
    if (!model.getSmartChargingService() && chargingLimitOutput) {
        model.setSmartChargingService(std::unique_ptr<SmartChargingService>(
            new SmartChargingService(*context, filesystem, MO_NUMCONNECTORS)));
    }

    if (auto scService = context->getModel().getSmartChargingService()) {
        scService->setSmartChargingOutput(connectorId, chargingLimitOutput);
        if (chargingLimitOutput) {
            scService->updateAllowedChargingRateUnit(true, true);
        } else {
            scService->updateAllowedChargingRateUnit(false, false);
        }
    }
}

void setEvReadyInput(std::function<bool()> evReadyInput, unsigned int connectorId) {
    if (!context) {
        MO_DBG_ERR("OCPP uninitialized"); //need to call mocpp_initialize before
        return;
    }
#if MO_ENABLE_V201
    if (context->getVersion().major == 2) {
        if (auto txService = context->getModel().getTransactionService()) {
            if (auto evse = txService->getEvse(connectorId)) {
                evse->setEvReadyInput(evReadyInput);
            }
        }
        return;
    }
#endif
    auto connector = context->getModel().getConnector(connectorId);
    if (!connector) {
        MO_DBG_ERR("could not find connector");
        return;
    }
    connector->setEvReadyInput(evReadyInput);
}

void setEvseReadyInput(std::function<bool()> evseReadyInput, unsigned int connectorId) {
    if (!context) {
        MO_DBG_ERR("OCPP uninitialized"); //need to call mocpp_initialize before
        return;
    }
#if MO_ENABLE_V201
    if (context->getVersion().major == 2) {
        if (auto txService = context->getModel().getTransactionService()) {
            if (auto evse = txService->getEvse(connectorId)) {
                evse->setEvseReadyInput(evseReadyInput);
            }
        }
        return;
    }
#endif
    auto connector = context->getModel().getConnector(connectorId);
    if (!connector) {
        MO_DBG_ERR("could not find connector");
        return;
    }
    connector->setEvseReadyInput(evseReadyInput);
}

void addErrorCodeInput(std::function<const char*()> errorCodeInput, unsigned int connectorId) {
    if (!context) {
        MO_DBG_ERR("OCPP uninitialized"); //need to call mocpp_initialize before
        return;
    }
    auto connector = context->getModel().getConnector(connectorId);
    if (!connector) {
        MO_DBG_ERR("could not find connector");
        return;
    }
    connector->addErrorCodeInput(errorCodeInput);
}

void addErrorDataInput(std::function<MicroOcpp::ErrorData()> errorDataInput, unsigned int connectorId) {
    if (!context) {
        MO_DBG_ERR("OCPP uninitialized"); //need to call mocpp_initialize before
        return;
    }
    auto connector = context->getModel().getConnector(connectorId);
    if (!connector) {
        MO_DBG_ERR("could not find connector");
        return;
    }
    connector->addErrorDataInput(errorDataInput);
}

void addMeterValueInput(std::function<float ()> valueInput, const char *measurand, const char *unit, const char *location, const char *phase, unsigned int connectorId) {
    if (!context) {
        MO_DBG_ERR("OCPP uninitialized"); //need to call mocpp_initialize before
        return;
    }

    if (!valueInput) {
        MO_DBG_ERR("value undefined");
        return;
    }

    if (!measurand) {
        measurand = "Energy.Active.Import.Register";
        MO_DBG_WARN("measurand unspecified; assume %s", measurand);
    }

    #if MO_ENABLE_V201
    if (context->getVersion().major == 2) {
        auto& model = context->getModel();
        if (!model.getMeteringServiceV201()) {
            model.setMeteringServiceV201(std::unique_ptr<Ocpp201::MeteringService>(
                new Ocpp201::MeteringService(context->getModel(), MO_NUM_EVSEID)));
        }
        if (auto mEvse = model.getMeteringServiceV201()->getEvse(connectorId)) {
            
            Ocpp201::SampledValueProperties properties;
            properties.setMeasurand(measurand); //mandatory for MO

            if (unit)
                properties.setUnitOfMeasureUnit(unit);
            if (location)
                properties.setLocation(location);
            if (phase)
                properties.setPhase(phase);

            mEvse->addMeterValueInput([valueInput] (ReadingContext) {return static_cast<double>(valueInput());}, properties);
        } else {
            MO_DBG_ERR("inalid arg");
        }
        return;
    }
    #endif

    SampledValueProperties properties;
    properties.setMeasurand(measurand); //mandatory for MO

    if (unit)
        properties.setUnit(unit);
    if (location)
        properties.setLocation(location);
    if (phase)
        properties.setPhase(phase);

    auto valueSampler = std::unique_ptr<MicroOcpp::SampledValueSamplerConcrete<float, MicroOcpp::SampledValueDeSerializer<float>>>(
                                    new MicroOcpp::SampledValueSamplerConcrete<float, MicroOcpp::SampledValueDeSerializer<float>>(
                properties,
                [valueInput] (ReadingContext) {return valueInput();}));
    addMeterValueInput(std::move(valueSampler), connectorId);
}

void addMeterValueInput(std::unique_ptr<SampledValueSampler> valueInput, unsigned int connectorId) {
    if (!context) {
        MO_DBG_ERR("OCPP uninitialized"); //need to call mocpp_initialize before
        return;
    }
    #if MO_ENABLE_V201
    if (context->getVersion().major == 2) {
        MO_DBG_ERR("addMeterValueInput(std::unique_ptr<SampledValueSampler>...) not compatible with v201. Use addMeterValueInput(std::function<float>...) instead");
        return;
    }
    #endif
    auto& model = context->getModel();
    if (!model.getMeteringService()) {
        model.setMeteringSerivce(std::unique_ptr<MeteringService>(
            new MeteringService(*context, MO_NUMCONNECTORS, filesystem)));
    }
    model.getMeteringService()->addMeterValueSampler(connectorId, std::move(valueInput));
}

void setOccupiedInput(std::function<bool()> occupied, unsigned int connectorId) {
    if (!context) {
        MO_DBG_ERR("OCPP uninitialized"); //need to call mocpp_initialize before
        return;
    }
#if MO_ENABLE_V201
    if (context->getVersion().major == 2) {
        if (auto availabilityService = context->getModel().getAvailabilityService()) {
            if (auto evse = availabilityService->getEvse(connectorId)) {
                evse->setOccupiedInput(occupied);
            }
        }
    }
#endif
    auto connector = context->getModel().getConnector(connectorId);
    if (!connector) {
        MO_DBG_ERR("could not find connector");
        return;
    }
    connector->setOccupiedInput(occupied);
}

void setStartTxReadyInput(std::function<bool()> startTxReady, unsigned int connectorId) {
    if (!context) {
        MO_DBG_ERR("OCPP uninitialized"); //need to call mocpp_initialize before
        return;
    }
    auto connector = context->getModel().getConnector(connectorId);
    if (!connector) {
        MO_DBG_ERR("could not find connector");
        return;
    }
    connector->setStartTxReadyInput(startTxReady);
}

void setStopTxReadyInput(std::function<bool()> stopTxReady, unsigned int connectorId) {
    if (!context) {
        MO_DBG_ERR("OCPP uninitialized"); //need to call mocpp_initialize before
        return;
    }
    auto connector = context->getModel().getConnector(connectorId);
    if (!connector) {
        MO_DBG_ERR("could not find connector");
        return;
    }
    connector->setStopTxReadyInput(stopTxReady);
}

void setTxNotificationOutput(std::function<void(MicroOcpp::Transaction*,MicroOcpp::TxNotification)> notificationOutput, unsigned int connectorId) {
    if (!context) {
        MO_DBG_ERR("OCPP uninitialized"); //need to call mocpp_initialize before
        return;
    }
    auto connector = context->getModel().getConnector(connectorId);
    if (!connector) {
        MO_DBG_ERR("could not find connector");
        return;
    }
    connector->setTxNotificationOutput(notificationOutput);
}

#if MO_ENABLE_CONNECTOR_LOCK
void setOnUnlockConnectorInOut(std::function<UnlockConnectorResult()> onUnlockConnectorInOut, unsigned int connectorId) {
    if (!context) {
        MO_DBG_ERR("OCPP uninitialized"); //need to call mocpp_initialize before
        return;
    }
    auto connector = context->getModel().getConnector(connectorId);
    if (!connector) {
        MO_DBG_ERR("could not find connector");
        return;
    }
    connector->setOnUnlockConnector(onUnlockConnectorInOut);
}
#endif //MO_ENABLE_CONNECTOR_LOCK

bool isOperative(unsigned int connectorId) {
    if (!context) {
        MO_DBG_WARN("OCPP uninitialized");
        return true; //assume "true" as default state
    }
#if MO_ENABLE_V201
    if (context->getVersion().major == 2) {
        if (auto availabilityService = context->getModel().getAvailabilityService()) {
            auto chargePoint = availabilityService->getEvse(OCPP_ID_OF_CP);
            auto connector = availabilityService->getEvse(connectorId);
            if (!chargePoint || !connector) {
                MO_DBG_ERR("could not find connector");
                return true; //assume "true" as default state
            }
            return chargePoint->isAvailable() && connector->isAvailable();
        }
    }
#endif
    auto& model = context->getModel();
    auto chargePoint = model.getConnector(OCPP_ID_OF_CP);
    auto connector = model.getConnector(connectorId);
    if (!chargePoint || !connector) {
        MO_DBG_ERR("could not find connector");
        return true; //assume "true" as default state
    }
    return chargePoint->isOperative() && connector->isOperative();
}

void setOnResetNotify(std::function<bool(bool)> onResetNotify) {
    if (!context) {
        MO_DBG_ERR("OCPP uninitialized"); //need to call mocpp_initialize before
        return;
    }

#if MO_ENABLE_V201
    if (context->getVersion().major == 2) {
        if (auto rService = context->getModel().getResetServiceV201()) {
            rService->setNotifyReset([onResetNotify] (ResetType) {return onResetNotify(true);});
        }
        return;
    }
#endif

    if (auto rService = context->getModel().getResetService()) {
        rService->setPreReset(onResetNotify);
    }
}

void setOnResetExecute(std::function<void(bool)> onResetExecute) {
    if (!context) {
        MO_DBG_ERR("OCPP uninitialized"); //need to call mocpp_initialize before
        return;
    }

#if MO_ENABLE_V201
    if (context->getVersion().major == 2) {
        if (auto rService = context->getModel().getResetServiceV201()) {
            rService->setExecuteReset([onResetExecute] () {onResetExecute(true); return true;});
        }
        return;
    }
#endif

    if (auto rService = context->getModel().getResetService()) {
        rService->setExecuteReset(onResetExecute);
    }
}

FirmwareService *getFirmwareService() {
    if (!context) {
        MO_DBG_ERR("OCPP uninitialized"); //need to call mocpp_initialize before
        return nullptr;
    }

    auto& model = context->getModel();
    if (!model.getFirmwareService()) {
        model.setFirmwareService(std::unique_ptr<FirmwareService>(
            new FirmwareService(*context)));
    }

    return model.getFirmwareService();
}

DiagnosticsService *getDiagnosticsService() {
    if (!context) {
        MO_DBG_ERR("OCPP uninitialized"); //need to call mocpp_initialize before
        return nullptr;
    }

    auto& model = context->getModel();
    if (!model.getDiagnosticsService()) {
        model.setDiagnosticsService(std::unique_ptr<DiagnosticsService>(
            new DiagnosticsService(*context)));
    }

    return model.getDiagnosticsService();
}

#if MO_ENABLE_CERT_MGMT

void setCertificateStore(std::unique_ptr<MicroOcpp::CertificateStore> certStore) {
    if (!context) {
        MO_DBG_ERR("OCPP uninitialized"); //need to call mocpp_initialize before
        return;
    }

    auto& model = context->getModel();
    if (!model.getCertificateService()) {
        model.setCertificateService(std::unique_ptr<CertificateService>(
            new CertificateService(*context)));
    }
    if (auto certService = model.getCertificateService()) {
        certService->setCertificateStore(std::move(certStore));
    } else {
        MO_DBG_ERR("OOM");
    }
}
#endif //MO_ENABLE_CERT_MGMT

Context *getOcppContext() {
    return context;
}

void setOnReceiveRequest(const char *operationType, OnReceiveReqListener onReceiveReq) {
    if (!context) {
        MO_DBG_ERR("OCPP uninitialized"); //need to call mocpp_initialize before
        return;
    }
    if (!operationType) {
        MO_DBG_ERR("invalid args");
        return;
    }
    context->getOperationRegistry().setOnRequest(operationType, onReceiveReq);
}

void setOnSendConf(const char *operationType, OnSendConfListener onSendConf) {
    if (!context) {
        MO_DBG_ERR("OCPP uninitialized"); //need to call mocpp_initialize before
        return;
    }
    if (!operationType) {
        MO_DBG_ERR("invalid args");
        return;
    }
    context->getOperationRegistry().setOnResponse(operationType, onSendConf);
}

void sendRequest(const char *operationType,
            std::function<std::unique_ptr<JsonDoc> ()> fn_createReq,
            std::function<void (JsonObject)> fn_processConf) {

    if (!context) {
        MO_DBG_ERR("OCPP uninitialized"); //need to call mocpp_initialize before
        return;
    }
    if (!operationType || !fn_createReq || !fn_processConf) {
        MO_DBG_ERR("invalid args");
        return;
    }

    auto request = makeRequest(new CustomOperation(operationType, fn_createReq, fn_processConf));
    context->initiateRequest(std::move(request));
}

void setRequestHandler(const char *operationType,
            std::function<void (JsonObject)> fn_processReq,
            std::function<std::unique_ptr<JsonDoc> ()> fn_createConf) {

    if (!context) {
        MO_DBG_ERR("OCPP uninitialized"); //need to call mocpp_initialize before
        return;
    }
    if (!operationType || !fn_processReq || !fn_createConf) {
        MO_DBG_ERR("invalid args");
        return;
    }

    auto captureOpType = makeString("MicroOcpp.cpp", operationType);

    context->getOperationRegistry().registerOperation(operationType, [captureOpType, fn_processReq, fn_createConf] () {
        return new CustomOperation(captureOpType.c_str(), fn_processReq, fn_createConf);
    });
}

void authorize(const char *idTag, OnReceiveConfListener onConf, OnAbortListener onAbort, OnTimeoutListener onTimeout, OnReceiveErrorListener onError, unsigned int timeout) {
    if (!context) {
        MO_DBG_ERR("OCPP uninitialized"); //need to call mocpp_initialize before
        return;
    }
    if (!idTag || strnlen(idTag, IDTAG_LEN_MAX + 2) > IDTAG_LEN_MAX) {
        MO_DBG_ERR("idTag format violation. Expect c-style string with at most %u characters", IDTAG_LEN_MAX);
        return;
    }
    auto authorize = makeRequest(
        new Authorize(context->getModel(), idTag));
    if (onConf)
        authorize->setOnReceiveConfListener(onConf);
    if (onAbort)
        authorize->setOnAbortListener(onAbort);
    if (onTimeout)
        authorize->setOnTimeoutListener(onTimeout);
    if (onError)
        authorize->setOnReceiveErrorListener(onError);
    if (timeout)
        authorize->setTimeout(timeout);
    else
        authorize->setTimeout(20000);
    context->initiateRequest(std::move(authorize));
}

bool startTransaction(const char *idTag, OnReceiveConfListener onConf, OnAbortListener onAbort, OnTimeoutListener onTimeout, OnReceiveErrorListener onError, unsigned int timeout) {
    if (!context) {
        MO_DBG_ERR("OCPP uninitialized"); //need to call mocpp_initialize before
        return false;
    }
    if (!idTag || strnlen(idTag, IDTAG_LEN_MAX + 2) > IDTAG_LEN_MAX) {
        MO_DBG_ERR("idTag format violation. Expect c-style string with at most %u characters", IDTAG_LEN_MAX);
        return false;
    }
    auto connector = context->getModel().getConnector(OCPP_ID_OF_CONNECTOR);
    if (!connector) {
        MO_DBG_ERR("could not find connector");
        return false;
    }
    auto transaction = connector->getTransaction();
    if (transaction) {
        if (transaction->getStartSync().isRequested()) {
            MO_DBG_ERR("transaction already in progress. Must call stopTransaction()");
            return false;
        }
        transaction->setIdTag(idTag);
    } else {
        beginTransaction_authorized(idTag); //request new transaction object
        transaction = connector->getTransaction();
        if (!transaction) {
            MO_DBG_WARN("transaction queue full");
            return false;
        }
    }

    if (auto mService = context->getModel().getMeteringService()) {
        auto meterStart = mService->readTxEnergyMeter(transaction->getConnectorId(), ReadingContext_TransactionBegin);
        if (meterStart && *meterStart) {
            transaction->setMeterStart(meterStart->toInteger());
        } else {
            MO_DBG_ERR("meterStart undefined");
        }
    }

    transaction->setStartTimestamp(context->getModel().getClock().now());

    transaction->commit();
    
    auto startTransaction = makeRequest(
        new StartTransaction(context->getModel(), transaction));
    if (onConf)
        startTransaction->setOnReceiveConfListener(onConf);
    if (onAbort)
        startTransaction->setOnAbortListener(onAbort);
    if (onTimeout)
        startTransaction->setOnTimeoutListener(onTimeout);
    if (onError)
        startTransaction->setOnReceiveErrorListener(onError);
    if (timeout)
        startTransaction->setTimeout(timeout);
    else
        startTransaction->setTimeout(0);
    context->initiateRequest(std::move(startTransaction));

    return true;
}

bool stopTransaction(OnReceiveConfListener onConf, OnAbortListener onAbort, OnTimeoutListener onTimeout, OnReceiveErrorListener onError, unsigned int timeout) {
    if (!context) {
        MO_DBG_ERR("OCPP uninitialized"); //need to call mocpp_initialize before
        return false;
    }
    auto connector = context->getModel().getConnector(OCPP_ID_OF_CONNECTOR);
    if (!connector) {
        MO_DBG_ERR("could not find connector");
        return false;
    }

    auto transaction = connector->getTransaction();
    if (!transaction || !transaction->isRunning()) {
        MO_DBG_ERR("no running Tx to stop");
        return false;
    }

    connector->endTransaction(transaction->getIdTag(), "Local");

    const char *idTag = transaction->getIdTag();
    if (idTag) {
        transaction->setStopIdTag(idTag);
    }

    if (auto mService = context->getModel().getMeteringService()) {
        auto meterStop = mService->readTxEnergyMeter(transaction->getConnectorId(), ReadingContext_TransactionEnd);
        if (meterStop && *meterStop) {
            transaction->setMeterStop(meterStop->toInteger());
        } else {
            MO_DBG_ERR("meterStop undefined");
        }
    }

    transaction->setStopTimestamp(context->getModel().getClock().now());

    transaction->commit();

    auto stopTransaction = makeRequest(
        new StopTransaction(context->getModel(), transaction));
    if (onConf)
        stopTransaction->setOnReceiveConfListener(onConf);
    if (onAbort)
        stopTransaction->setOnAbortListener(onAbort);
    if (onTimeout)
        stopTransaction->setOnTimeoutListener(onTimeout);
    if (onError)
        stopTransaction->setOnReceiveErrorListener(onError);
    if (timeout)
        stopTransaction->setTimeout(timeout);
    else
        stopTransaction->setTimeout(0);
    context->initiateRequest(std::move(stopTransaction));

    return true;
}
