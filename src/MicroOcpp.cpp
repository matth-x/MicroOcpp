// matth-x/MicroOcpp
// Copyright Matthias Akstaller 2019 - 2023
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
#include <MicroOcpp/Core/SimpleRequestFactory.h>
#include <MicroOcpp/Core/OperationRegistry.h>
#include <MicroOcpp/Core/FilesystemAdapter.h>
#include <MicroOcpp/Core/FilesystemUtils.h>

#include <MicroOcpp/Operations/Authorize.h>
#include <MicroOcpp/Operations/StartTransaction.h>
#include <MicroOcpp/Operations/StopTransaction.h>
#include <MicroOcpp/Operations/CustomOperation.h>

#include <MicroOcpp/Debug.h>
#include <MicroOcpp/Version.h>

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

using namespace MicroOcpp;
using namespace MicroOcpp::Facade;
using namespace MicroOcpp::Ocpp16;

#ifndef MO_CUSTOM_WS
void mocpp_initialize(const char *CS_hostname, uint16_t CS_port, const char *CS_url, const char *chargePointModel, const char *chargePointVendor, FilesystemOpt fsOpt, const char *login, const char *password, const char *CA_cert, bool autoRecover) {
    if (context) {
        MO_DBG_WARN("already initialized. To reinit, call mocpp_deinitialize() before");
        return;
    }

    if (!webSocket)
        webSocket = new WebSocketsClient();

    if (!strncmp(CS_url,"wss",3) || !strncmp(CS_url,"https",5))
    {
        // server address, port, URL and TLS certificate
        webSocket->beginSslWithCA(CS_hostname, CS_port, CS_url, CA_cert, "ocpp1.6");
    }
    else
    {
        // server address, port, URL
        webSocket->begin(CS_hostname, CS_port, CS_url, "ocpp1.6");
    }

    // try ever 5000 again if connection has failed
    webSocket->setReconnectInterval(5000);

    // start heartbeat (optional)
    // ping server every 15000 ms
    // expect pong from server within 3000 ms
    // consider connection disconnected if pong is not received 2 times
    webSocket->enableHeartbeat(15000, 3000, 2); //comment this one out to for specific OCPP servers

    // add authentication data (optional)
    if (login && password && strlen(login) + strlen(password) >= 2) {
        webSocket->setAuthorization(login, password);
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

void mocpp_initialize(Connection& connection, const char *bootNotificationCredentials, std::shared_ptr<FilesystemAdapter> fs, bool autoRecover) {
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
        MO_DBG_ERR("multiple initialization failures detected");
        if (filesystem) {
            bool success = FilesystemUtils::remove_if(filesystem, [] (const char *fname) -> bool {
                return !strncmp(fname, "sd", strlen("sd")) ||
                       !strncmp(fname, "tx", strlen("tx")) ||
                       !strncmp(fname, "op", strlen("op")) ||
                       !strncmp(fname, "sc-", strlen("sc-")) ||
                       !strncmp(fname, "reservation", strlen("reservation"));
            });
            MO_DBG_ERR("clear local state files (recovery): %s", success ? "success" : "not completed");

            bootstats = BootStats();
        }
    }

    bootstats.bootNr++; //assign new boot number to this run
    BootService::storeBootStats(filesystem, bootstats);

    configuration_init(filesystem); //call before each other library call

    context = new Context(connection, filesystem, bootstats.bootNr);
    auto& model = context->getModel();

    model.setTransactionStore(std::unique_ptr<TransactionStore>(
        new TransactionStore(MO_NUMCONNECTORS, filesystem)));
    model.setBootService(std::unique_ptr<BootService>(
        new BootService(*context, filesystem)));
    model.setConnectorsCommon(std::unique_ptr<ConnectorsCommon>(
        new ConnectorsCommon(*context, MO_NUMCONNECTORS, filesystem)));
    std::vector<std::unique_ptr<Connector>> connectors;
    for (unsigned int connectorId = 0; connectorId < MO_NUMCONNECTORS; connectorId++) {
        connectors.emplace_back(new Connector(*context, connectorId));
    }
    model.setConnectors(std::move(connectors));
    model.setHeartbeatService(std::unique_ptr<HeartbeatService>(
        new HeartbeatService(*context)));
    model.setAuthorizationService(std::unique_ptr<AuthorizationService>(
        new AuthorizationService(*context, filesystem)));
    model.setReservationService(std::unique_ptr<ReservationService>(
        new ReservationService(*context, MO_NUMCONNECTORS)));
    model.setResetService(std::unique_ptr<ResetService>(
        new ResetService(*context)));

#if !defined(MO_CUSTOM_UPDATER) && !defined(MO_CUSTOM_WS)
    model.setFirmwareService(std::unique_ptr<FirmwareService>(
        EspWiFi::makeFirmwareService(*context))); //instantiate FW service + ESP installation routine
#else
    model.setFirmwareService(std::unique_ptr<FirmwareService>(
        new FirmwareService(*context))); //only instantiate FW service
#endif

#if !defined(MO_CUSTOM_DIAGNOSTICS) && !defined(MO_CUSTOM_WS)
    model.setDiagnosticsService(std::unique_ptr<DiagnosticsService>(
        EspWiFi::makeDiagnosticsService(*context))); //will only return "Rejected" because client needs to implement logging
#else
    model.setDiagnosticsService(std::unique_ptr<DiagnosticsService>(
        new DiagnosticsService(*context)));
#endif

#if MO_PLATFORM == MO_PLATFORM_ARDUINO && (defined(ESP32) || defined(ESP8266))
    if (!model.getResetService()->getExecuteReset())
        model.getResetService()->setExecuteReset(makeDefaultResetFn());
#endif

    model.getBootService()->setChargePointCredentials(bootNotificationCredentials);

    auto credsJson = model.getBootService()->getChargePointCredentials();
    if (credsJson && credsJson->containsKey("firmwareVersion")) {
        model.getFirmwareService()->setBuildNumber((*credsJson)["firmwareVersion"]);
    }
    credsJson.reset();

    configuration_load();

    MO_DBG_INFO("initialized MicroOcpp v" MO_VERSION);
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
        if (!idTag || !strcmp(idTag, getTransactionIdTag())) {
            res = endTransaction_authorized(idTag, reason, connectorId);
        } else {
            MO_DBG_INFO("endTransaction: idTag doesn't match");
            (void)0;
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
    auto connector = context->getModel().getConnector(connectorId);
    if (!connector) {
        MO_DBG_ERR("could not find connector");
        return false;
    }
    return connector->ocppPermitsCharge();
}

void setConnectorPluggedInput(std::function<bool()> pluggedInput, unsigned int connectorId) {
    if (!context) {
        MO_DBG_ERR("OCPP uninitialized"); //need to call mocpp_initialize before
        return;
    }
    auto connector = context->getModel().getConnector(connectorId);
    if (!connector) {
        MO_DBG_ERR("could not find connector");
        return;
    }
    connector->setConnectorPluggedInput(pluggedInput);
}

void setEnergyMeterInput(std::function<float()> energyInput, unsigned int connectorId) {
    if (!context) {
        MO_DBG_ERR("OCPP uninitialized"); //need to call mocpp_initialize before
        return;
    }
    auto& model = context->getModel();
    if (!model.getMeteringService()) {
        model.setMeteringSerivce(std::unique_ptr<MeteringService>(
            new MeteringService(*context, MO_NUMCONNECTORS, filesystem)));
    }
    SampledValueProperties meterProperties;
    meterProperties.setMeasurand("Energy.Active.Import.Register");
    meterProperties.setUnit("Wh");
    auto mvs = std::unique_ptr<SampledValueSamplerConcrete<float, SampledValueDeSerializer<float>>>(
                           new SampledValueSamplerConcrete<float, SampledValueDeSerializer<float>>(
            meterProperties,
            [energyInput] (ReadingContext) {return energyInput();}
    ));
    model.getMeteringService()->addMeterValueSampler(connectorId, std::move(mvs));
}

void setPowerMeterInput(std::function<float()> powerInput, unsigned int connectorId) {
    if (!context) {
        MO_DBG_ERR("OCPP uninitialized"); //need to call mocpp_initialize before
        return;
    }

    auto& model = context->getModel();
    if (!model.getMeteringService()) {
        model.setMeteringSerivce(std::unique_ptr<MeteringService>(
            new MeteringService(*context, MO_NUMCONNECTORS, filesystem)));
    }
    SampledValueProperties meterProperties;
    meterProperties.setMeasurand("Power.Active.Import");
    meterProperties.setUnit("W");
    auto mvs = std::unique_ptr<SampledValueSamplerConcrete<float, SampledValueDeSerializer<float>>>(
                           new SampledValueSamplerConcrete<float, SampledValueDeSerializer<float>>(
            meterProperties,
            [powerInput] (ReadingContext) {return powerInput();}
    ));
    model.getMeteringService()->addMeterValueSampler(connectorId, std::move(mvs));
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
                [valueInput] (MicroOcpp::ReadingContext) {return valueInput();}));
    addMeterValueInput(std::move(valueSampler), connectorId);
}

void addMeterValueInput(std::unique_ptr<SampledValueSampler> valueInput, unsigned int connectorId) {
    if (!context) {
        MO_DBG_ERR("OCPP uninitialized"); //need to call mocpp_initialize before
        return;
    }
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

void setOnUnlockConnectorInOut(std::function<PollResult<bool>()> onUnlockConnectorInOut, unsigned int connectorId) {
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

bool isOperative(unsigned int connectorId) {
    if (!context) {
        MO_DBG_WARN("OCPP uninitialized");
        return true; //assume "true" as default state
    }
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

    if (auto rService = context->getModel().getResetService()) {
        rService->setPreReset(onResetNotify);
    }
}

void setOnResetExecute(std::function<void(bool)> onResetExecute) {
    if (!context) {
        MO_DBG_ERR("OCPP uninitialized"); //need to call mocpp_initialize before
        return;
    }

    if (auto rService = context->getModel().getResetService()) {
        rService->setExecuteReset(onResetExecute);
    }
}

#if defined(MO_CUSTOM_UPDATER) || defined(MO_CUSTOM_WS)
FirmwareService *getFirmwareService() {
    auto& model = context->getModel();
    return model.getFirmwareService();
}
#endif

#if defined(MO_CUSTOM_DIAGNOSTICS) || defined(MO_CUSTOM_WS)
DiagnosticsService *getDiagnosticsService() {
    auto& model = context->getModel();
    return model.getDiagnosticsService();
}
#endif

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
            std::function<std::unique_ptr<DynamicJsonDocument> ()> fn_createReq,
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
            std::function<std::unique_ptr<DynamicJsonDocument> ()> fn_createConf) {

    if (!context) {
        MO_DBG_ERR("OCPP uninitialized"); //need to call mocpp_initialize before
        return;
    }
    if (!operationType || !fn_processReq || !fn_createConf) {
        MO_DBG_ERR("invalid args");
        return;
    }

    std::string captureOpType = operationType;

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
        auto meterStart = mService->readTxEnergyMeter(transaction->getConnectorId(), ReadingContext::TransactionBegin);
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
        auto meterStop = mService->readTxEnergyMeter(transaction->getConnectorId(), ReadingContext::TransactionEnd);
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
