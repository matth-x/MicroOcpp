// matth-x/ArduinoOcpp
// Copyright Matthias Akstaller 2019 - 2023
// MIT License

#include "ArduinoOcpp.h"

#include <ArduinoOcpp/Context.h>
#include <ArduinoOcpp/Model/Model.h>
#include <ArduinoOcpp/Model/Metering/MeteringService.h>
#include <ArduinoOcpp/Model/SmartCharging/SmartChargingService.h>
#include <ArduinoOcpp/Model/ChargeControl/ChargeControlCommon.h>
#include <ArduinoOcpp/Model/Heartbeat/HeartbeatService.h>
#include <ArduinoOcpp/Model/FirmwareManagement/FirmwareService.h>
#include <ArduinoOcpp/Model/Diagnostics/DiagnosticsService.h>
#include <ArduinoOcpp/Model/Transactions/TransactionStore.h>
#include <ArduinoOcpp/Model/Authorization/AuthorizationService.h>
#include <ArduinoOcpp/Model/Reservation/ReservationService.h>
#include <ArduinoOcpp/Model/Boot/BootService.h>
#include <ArduinoOcpp/Model/Reset/ResetService.h>
#include <ArduinoOcpp/Core/SimpleRequestFactory.h>
#include <ArduinoOcpp/Core/OperationRegistry.h>
#include <ArduinoOcpp/Core/Configuration.h>
#include <ArduinoOcpp/Core/FilesystemAdapter.h>
#include <ArduinoOcpp/Core/FilesystemUtils.h>

#include <ArduinoOcpp/Operations/Authorize.h>
#include <ArduinoOcpp/Operations/StartTransaction.h>
#include <ArduinoOcpp/Operations/StopTransaction.h>
#include <ArduinoOcpp/Operations/CustomOperation.h>

#include <ArduinoOcpp/Debug.h>

namespace ArduinoOcpp {
namespace Facade {

#ifndef AO_CUSTOM_WS
WebSocketsClient *webSocket {nullptr};
Connection *connection {nullptr};
#endif

Context *context {nullptr};
std::shared_ptr<FilesystemAdapter> filesystem;

#ifndef AO_NUMCONNECTORS
#define AO_NUMCONNECTORS 2
#endif

#define OCPP_ID_OF_CP 0
#define OCPP_ID_OF_CONNECTOR 1

} //end namespace ArduinoOcpp::Facade
} //end namespace ArduinoOcpp

using namespace ArduinoOcpp;
using namespace ArduinoOcpp::Facade;
using namespace ArduinoOcpp::Ocpp16;

#ifndef AO_CUSTOM_WS
void OCPP_initialize(const char *CS_hostname, uint16_t CS_port, const char *CS_url, const char *chargePointModel, const char *chargePointVendor, FilesystemOpt fsOpt) {
    if (context) {
        AO_DBG_WARN("Can't be called two times. Either restart ESP, or call OCPP_deinitialize() before");
        return;
    }

    if (!webSocket)
        webSocket = new WebSocketsClient();

    // server address, port and URL
    webSocket->begin(CS_hostname, CS_port, CS_url, "ocpp1.6");

    // try ever 5000 again if connection has failed
    webSocket->setReconnectInterval(5000);

    // start heartbeat (optional)
    // ping server every 15000 ms
    // expect pong from server within 3000 ms
    // consider connection disconnected if pong is not received 2 times
    webSocket->enableHeartbeat(15000, 3000, 2); //comment this one out to for specific OCPP servers

    delete connection;
    connection = new EspWiFi::WSClient(webSocket);

    OCPP_initialize(*connection, ChargerCredentials(chargePointModel, chargePointVendor), fsOpt);
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
        AO_DBG_ERR("Charger Credentials too long");
    }

    size_t written = serializeJson(creds, payload, 512);

    if (written < 2) {
        AO_DBG_ERR("Charger Credentials could not be written");
        sprintf(payload, "{}");
    }
}

void OCPP_initialize(Connection& connection, const char *bootNotificationCredentials, FilesystemOpt fsOpt) {
    if (context) {
        AO_DBG_WARN("Can't be called two times. To change the credentials, either restart ESP, or call OCPP_deinitialize() before");
        return;
    }

#ifndef AO_DEACTIVATE_FLASH
    filesystem = makeDefaultFilesystemAdapter(fsOpt);
#endif
    AO_DBG_DEBUG("filesystem %s", filesystem ? "loaded" : "error");

    BootStats bootstats;
    BootService::loadBootStats(filesystem, bootstats);

    if (bootstats.getBootFailureCount() > 3) {
        AO_DBG_ERR("multiple initialization failures detected");
        if (filesystem) {
            bool success = FilesystemUtils::remove_if(filesystem, [] (const char *fname) -> bool {
                return strcmp(fname, "ocpp-creds.jsn");
            });
            AO_DBG_ERR("clear local state folder (except WS creds): %s", success ? "success" : "not completed");

            bootstats = BootStats();
        }
    }

    bootstats.bootNr++; //assign new boot number to this run
    BootService::storeBootStats(filesystem, bootstats);
    
    configuration_init(filesystem); //call before each other library call

    context = new Context(connection, filesystem, bootstats.bootNr);
    auto& model = context->getModel();

    model.setTransactionStore(std::unique_ptr<TransactionStore>(
        new TransactionStore(AO_NUMCONNECTORS, filesystem)));
    model.setBootService(std::unique_ptr<BootService>(
        new BootService(*context, filesystem)));
    model.setChargeControlCommon(std::unique_ptr<ChargeControlCommon>(
        new ChargeControlCommon(*context, AO_NUMCONNECTORS, filesystem)));
    std::vector<Connector> connectors;
    for (unsigned int connectorId = 0; connectorId < AO_NUMCONNECTORS; connectorId++) {
        connectors.push_back(Connector(*context, connectorId));
    }
    model.setConnectors(std::move(connectors));
    model.setHeartbeatService(std::unique_ptr<HeartbeatService>(
        new HeartbeatService(*context)));
    model.setAuthorizationService(std::unique_ptr<AuthorizationService>(
        new AuthorizationService(*context, filesystem)));
    model.setReservationService(std::unique_ptr<ReservationService>(
        new ReservationService(*context, AO_NUMCONNECTORS)));
    model.setResetService(std::unique_ptr<ResetService>(
        new ResetService(*context)));

#if !defined(AO_CUSTOM_UPDATER) && !defined(AO_CUSTOM_WS)
    model.setFirmwareService(std::unique_ptr<FirmwareService>(
        EspWiFi::makeFirmwareService(*context))); //instantiate FW service + ESP installation routine
#else
    model.setFirmwareService(std::unique_ptr<FirmwareService>(
        new FirmwareService(*context))); //only instantiate FW service
#endif

#if !defined(AO_CUSTOM_DIAGNOSTICS) && !defined(AO_CUSTOM_WS)
    model.setDiagnosticsService(std::unique_ptr<DiagnosticsService>(
        EspWiFi::makeDiagnosticsService(*context))); //will only return "Rejected" because client needs to implement logging
#else
    model.setDiagnosticsService(std::unique_ptr<DiagnosticsService>(
        new DiagnosticsService(*context)));
#endif

#if AO_PLATFORM == AO_PLATFORM_ARDUINO && (defined(ESP32) || defined(ESP8266))
    if (!model.getResetService()->getExecuteReset())
        model.getResetService()->setExecuteReset(makeDefaultResetFn());
#endif

    model.getBootService()->setChargePointCredentials(bootNotificationCredentials);

    auto credsJson = model.getBootService()->getChargePointCredentials();
    if (credsJson && credsJson->containsKey("firmwareVersion")) {
        model.getFirmwareService()->setBuildNumber((*credsJson)["firmwareVersion"]);
    }
    credsJson.reset();
}

void OCPP_deinitialize() {

    if (context) {
        //release bootstats recovery mechanism
        BootStats bootstats;
        BootService::loadBootStats(filesystem, bootstats);
        if (bootstats.lastBootSuccess != bootstats.bootNr) {
            AO_DBG_DEBUG("boot success timer override");
            bootstats.lastBootSuccess = bootstats.bootNr;
            BootService::storeBootStats(filesystem, bootstats);
        }
    }
    
    delete context;
    context = nullptr;

#ifndef AO_CUSTOM_WS
    delete connection;
    connection = nullptr;
    delete webSocket;
    webSocket = nullptr;
#endif

    filesystem.reset();

    configuration_deinit();
}

void OCPP_loop() {
    if (!context) {
        AO_DBG_WARN("Please call OCPP_initialize before");
        return;
    }

    context->loop();
}

std::shared_ptr<Transaction> beginTransaction(const char *idTag, unsigned int connectorId) {
    if (!context) {
        AO_DBG_ERR("OCPP uninitialized"); //please call OCPP_initialize before
        return nullptr;
    }
    if (!idTag || strnlen(idTag, IDTAG_LEN_MAX + 2) > IDTAG_LEN_MAX) {
        AO_DBG_ERR("idTag format violation. Expect c-style string with at most %u characters", IDTAG_LEN_MAX);
        return nullptr;
    }
    auto connector = context->getModel().getConnector(connectorId);
    if (!connector) {
        AO_DBG_ERR("Could not find connector. Ignore");
        return nullptr;
    }

    return connector->beginTransaction(idTag);
}

std::shared_ptr<Transaction> beginTransaction_authorized(const char *idTag, const char *parentIdTag, unsigned int connectorId) {
    if (!context) {
        AO_DBG_ERR("OCPP uninitialized"); //please call OCPP_initialize before
        return nullptr;
    }
    if (!idTag || strnlen(idTag, IDTAG_LEN_MAX + 2) > IDTAG_LEN_MAX ||
        (parentIdTag && strnlen(parentIdTag, IDTAG_LEN_MAX + 2) > IDTAG_LEN_MAX)) {
        AO_DBG_ERR("(parent)idTag format violation. Expect c-style string with at most %u characters", IDTAG_LEN_MAX);
        return nullptr;
    }
    auto connector = context->getModel().getConnector(connectorId);
    if (!connector) {
        AO_DBG_ERR("Could not find connector. Ignore");
        return nullptr;
    }
    
    return connector->beginTransaction_authorized(idTag, parentIdTag);
}

bool endTransaction(const char *reason, unsigned int connectorId) {
    if (!context) {
        AO_DBG_ERR("OCPP uninitialized"); //please call OCPP_initialize before
        return false;
    }
    auto connector = context->getModel().getConnector(connectorId);
    if (!connector) {
        AO_DBG_ERR("Could not find connector. Ignore");
        return false;
    }
    auto res = isTransactionActive(connectorId);
    connector->endTransaction(reason);
    return res;
}

bool isTransactionActive(unsigned int connectorId) {
    if (!context) {
        AO_DBG_ERR("OCPP uninitialized"); //please call OCPP_initialize before
        return false;
    }
    auto connector = context->getModel().getConnector(connectorId);
    if (!connector) {
        AO_DBG_ERR("Could not find connector. Ignore");
        return false;
    }
    auto& tx = connector->getTransaction();
    return tx ? tx->isActive() : false;
}

bool isTransactionRunning(unsigned int connectorId) {
    if (!context) {
        AO_DBG_ERR("OCPP uninitialized"); //please call OCPP_initialize before
        return false;
    }
    auto connector = context->getModel().getConnector(connectorId);
    if (!connector) {
        AO_DBG_ERR("Could not find connector. Ignore");
        return false;
    }
    auto& tx = connector->getTransaction();
    return tx ? tx->isRunning() : false;
}

bool ocppPermitsCharge(unsigned int connectorId) {
    if (!context) {
        AO_DBG_WARN("Please call OCPP_initialize before");
        return false;
    }
    auto connector = context->getModel().getConnector(connectorId);
    if (!connector) {
        AO_DBG_ERR("Could not find connector. Ignore");
        return false;
    }
    return connector->ocppPermitsCharge();
}

void setConnectorPluggedInput(std::function<bool()> pluggedInput, unsigned int connectorId) {
    if (!context) {
        AO_DBG_ERR("OCPP uninitialized"); //please call OCPP_initialize before
        return;
    }
    auto connector = context->getModel().getConnector(connectorId);
    if (!connector) {
        AO_DBG_ERR("Could not find connector. Ignore");
        return;
    }
    connector->setConnectorPluggedInput(pluggedInput);

    if (pluggedInput) {
        AO_DBG_INFO("Added ConnectorPluggedInput. Transaction-management is in auto mode now");
    } else {
        AO_DBG_INFO("Reset ConnectorPluggedInput. Transaction-management is in manual mode now");
    }
}

void setEnergyMeterInput(std::function<float()> energyInput, unsigned int connectorId) {
    if (!context) {
        AO_DBG_ERR("OCPP uninitialized"); //please call OCPP_initialize before
        return;
    }
    auto& model = context->getModel();
    if (!model.getMeteringService()) {
        model.setMeteringSerivce(std::unique_ptr<MeteringService>(
            new MeteringService(*context, AO_NUMCONNECTORS, filesystem)));
    }
    SampledValueProperties meterProperties;
    meterProperties.setMeasurand("Energy.Active.Import.Register");
    meterProperties.setUnit("Wh");
    auto mvs = std::unique_ptr<SampledValueSamplerConcrete<int32_t, SampledValueDeSerializer<int32_t>>>(
                           new SampledValueSamplerConcrete<int32_t, SampledValueDeSerializer<int32_t>>(
            meterProperties,
            [energyInput] (ReadingContext) {return energyInput();}
    ));
    model.getMeteringService()->addMeterValueSampler(connectorId, std::move(mvs));
}

void setPowerMeterInput(std::function<float()> powerInput, unsigned int connectorId) {
    if (!context) {
        AO_DBG_ERR("OCPP uninitialized"); //please call OCPP_initialize before
        return;
    }

    auto& model = context->getModel();
    if (!model.getMeteringService()) {
        model.setMeteringSerivce(std::unique_ptr<MeteringService>(
            new MeteringService(*context, AO_NUMCONNECTORS, filesystem)));
    }
    SampledValueProperties meterProperties;
    meterProperties.setMeasurand("Power.Active.Import");
    meterProperties.setUnit("W");
    auto mvs = std::unique_ptr<SampledValueSamplerConcrete<int32_t, SampledValueDeSerializer<int32_t>>>(
                           new SampledValueSamplerConcrete<int32_t, SampledValueDeSerializer<int32_t>>(
            meterProperties,
            [powerInput] (ReadingContext) {return powerInput();}
    ));
    model.getMeteringService()->addMeterValueSampler(connectorId, std::move(mvs));
}

void setSmartChargingPowerOutput(std::function<void(float)> chargingLimitOutput, unsigned int connectorId) {
    if (!context) {
        AO_DBG_ERR("OCPP uninitialized"); //please call OCPP_initialize before
        return;
    }
    if (!context->getModel().getConnector(connectorId)) {
        AO_DBG_ERR("Could not find connector. Ignore");
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
        AO_DBG_ERR("OCPP uninitialized"); //please call OCPP_initialize before
        return;
    }
    if (!context->getModel().getConnector(connectorId)) {
        AO_DBG_ERR("Could not find connector. Ignore");
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
        AO_DBG_ERR("OCPP uninitialized"); //please call OCPP_initialize before
        return;
    }
    if (!context->getModel().getConnector(connectorId)) {
        AO_DBG_ERR("Could not find connector. Ignore");
        return;
    }

    auto& model = context->getModel();
    if (!model.getSmartChargingService() && chargingLimitOutput) {
        model.setSmartChargingService(std::unique_ptr<SmartChargingService>(
            new SmartChargingService(*context, filesystem, AO_NUMCONNECTORS)));
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
        AO_DBG_ERR("OCPP uninitialized"); //please call OCPP_initialize before
        return;
    }
    auto connector = context->getModel().getConnector(connectorId);
    if (!connector) {
        AO_DBG_ERR("Could not find connector. Ignore");
        return;
    }
    connector->setEvReadyInput(evReadyInput);
}

void setEvseReadyInput(std::function<bool()> evseReadyInput, unsigned int connectorId) {
    if (!context) {
        AO_DBG_ERR("OCPP uninitialized"); //please call OCPP_initialize before
        return;
    }
    auto connector = context->getModel().getConnector(connectorId);
    if (!connector) {
        AO_DBG_ERR("Could not find connector. Ignore");
        return;
    }
    connector->setEvseReadyInput(evseReadyInput);
}

void addErrorCodeInput(std::function<ArduinoOcpp::ErrorCode()> errorCodeInput, unsigned int connectorId) {
    if (!context) {
        AO_DBG_ERR("OCPP uninitialized"); //please call OCPP_initialize before
        return;
    }
    auto connector = context->getModel().getConnector(connectorId);
    if (!connector) {
        AO_DBG_ERR("Could not find connector. Ignore");
        return;
    }
    connector->addErrorCodeInput(errorCodeInput);
}

void addErrorDataInput(std::function<ArduinoOcpp::ErrorData()> errorDataInput, unsigned int connectorId) {
    if (!context) {
        AO_DBG_ERR("OCPP uninitialized"); //please call OCPP_initialize before
        return;
    }
    auto connector = context->getModel().getConnector(connectorId);
    if (!connector) {
        AO_DBG_ERR("Could not find connector. Ignore");
        return;
    }
    connector->addErrorDataInput(errorDataInput);
}

void addMeterValueInput(std::function<int32_t ()> valueInput, const char *measurand, const char *unit, const char *location, const char *phase, unsigned int connectorId) {
    if (!context) {
        AO_DBG_ERR("OCPP uninitialized"); //please call OCPP_initialize before
        return;
    }

    if (!valueInput) {
        AO_DBG_ERR("value undefined");
        return;
    }

    if (!measurand) {
        measurand = "Energy.Active.Import.Register";
        AO_DBG_WARN("Measurand unspecified; assume %s", measurand);
    }

    SampledValueProperties properties;
    properties.setMeasurand(measurand); //mandatory for AO

    if (unit)
        properties.setUnit(unit);
    if (location)
        properties.setLocation(location);
    if (phase)
        properties.setPhase(phase);

    auto valueSampler = std::unique_ptr<SampledValueSamplerConcrete<int32_t, SampledValueDeSerializer<int32_t>>>(
                                    new SampledValueSamplerConcrete<int32_t, SampledValueDeSerializer<int32_t>>(
                properties,
                [valueInput] (ReadingContext) -> int32_t {return valueInput();}));
    addMeterValueInput(std::move(valueSampler), connectorId);
}

void addMeterValueInput(std::unique_ptr<SampledValueSampler> valueInput, unsigned int connectorId) {
    if (!context) {
        AO_DBG_ERR("OCPP uninitialized"); //please call OCPP_initialize before
        return;
    }
    auto& model = context->getModel();
    if (!model.getMeteringService()) {
        model.setMeteringSerivce(std::unique_ptr<MeteringService>(
            new MeteringService(*context, AO_NUMCONNECTORS, filesystem)));
    }
    model.getMeteringService()->addMeterValueSampler(connectorId, std::move(valueInput));
}

void setOnResetNotify(std::function<bool(bool)> onResetNotify) {
    if (!context) {
        AO_DBG_ERR("OCPP uninitialized"); //please call OCPP_initialize before
        return;
    }

    if (auto rService = context->getModel().getResetService()) {
        rService->setPreReset(onResetNotify);
    }
}

void setOnResetExecute(std::function<void(bool)> onResetExecute) {
    if (!context) {
        AO_DBG_ERR("OCPP uninitialized"); //please call OCPP_initialize before
        return;
    }

    if (auto rService = context->getModel().getResetService()) {
        rService->setExecuteReset(onResetExecute);
    }
}

void setOnUnlockConnectorInOut(std::function<PollResult<bool>()> onUnlockConnectorInOut, unsigned int connectorId) {
    if (!context) {
        AO_DBG_ERR("OCPP uninitialized"); //please call OCPP_initialize before
        return;
    }
    auto connector = context->getModel().getConnector(connectorId);
    if (!connector) {
        AO_DBG_ERR("Could not find connector. Ignore");
        return;
    }
    connector->setOnUnlockConnector(onUnlockConnectorInOut);
}

void setStartTxReadyInput(std::function<bool()> startTxReady, unsigned int connectorId) {
    if (!context) {
        AO_DBG_ERR("OCPP uninitialized"); //please call OCPP_initialize before
        return;
    }
    auto connector = context->getModel().getConnector(connectorId);
    if (!connector) {
        AO_DBG_ERR("Could not find connector. Ignore");
        return;
    }
    connector->setStartTxReadyInput(startTxReady);
}

void setStopTxReadyInput(std::function<bool()> stopTxReady, unsigned int connectorId) {
    if (!context) {
        AO_DBG_ERR("OCPP uninitialized"); //please call OCPP_initialize before
        return;
    }
    auto connector = context->getModel().getConnector(connectorId);
    if (!connector) {
        AO_DBG_ERR("Could not find connector. Ignore");
        return;
    }
    connector->setStopTxReadyInput(stopTxReady);
}

void setOccupiedInput(std::function<bool()> occupied, unsigned int connectorId) {
    if (!context) {
        AO_DBG_ERR("OCPP uninitialized"); //please call OCPP_initialize before
        return;
    }
    auto connector = context->getModel().getConnector(connectorId);
    if (!connector) {
        AO_DBG_ERR("Could not find connector. Ignore");
        return;
    }
    connector->setOccupiedInput(occupied);
}

bool isOperative(unsigned int connectorId) {
    if (!context) {
        AO_DBG_WARN("Please call OCPP_initialize before");
        return true; //assume "true" as default state
    }
    auto& model = context->getModel();
    auto chargePoint = model.getConnector(OCPP_ID_OF_CP);
    auto connector = model.getConnector(connectorId);
    if (!chargePoint || !connector) {
        AO_DBG_ERR("Could not find connector. Ignore");
        return true; //assume "true" as default state
    }
    return chargePoint->isOperative() && connector->isOperative();
}

std::shared_ptr<Transaction> ao_undefinedTx;

std::shared_ptr<Transaction>& getTransaction(unsigned int connectorId) {
    if (!context) {
        AO_DBG_WARN("Please call OCPP_initialize before");
        return ao_undefinedTx;
    }
    auto connector = context->getModel().getConnector(connectorId);
    if (!connector) {
        AO_DBG_ERR("Could not find connector. Ignore");
        return ao_undefinedTx;
    }
    return connector->getTransaction();
}

const char *getTransactionIdTag(unsigned int connectorId) {
    if (!context) {
        AO_DBG_ERR("OCPP uninitialized"); //please call OCPP_initialize before
        return nullptr;
    }
    auto connector = context->getModel().getConnector(connectorId);
    if (!connector) {
        AO_DBG_ERR("Could not find connector. Ignore");
        return nullptr;
    }
    auto& tx = connector->getTransaction();
    return tx ? tx->getIdTag() : nullptr;
}

void setTxNotificationOutput(std::function<void(ArduinoOcpp::TxNotification,ArduinoOcpp::Transaction*)> notificationOutput, unsigned int connectorId) {
    if (!context) {
        AO_DBG_ERR("OCPP uninitialized"); //please call OCPP_initialize before
        return;
    }
    auto connector = context->getModel().getConnector(connectorId);
    if (!connector) {
        AO_DBG_ERR("Could not find connector. Ignore");
        return;
    }
    connector->setTxNotificationOutput(notificationOutput);
}

bool isBlockedByReservation(const char *idTag, unsigned int connectorId) {
    if (!context) {
        AO_DBG_WARN("Please call OCPP_initialize before");
        return false;
    }
    if (!idTag || strnlen(idTag, IDTAG_LEN_MAX + 2) > IDTAG_LEN_MAX) {
        AO_DBG_ERR("idTag format violation. Expect c-style string with at most %u characters", IDTAG_LEN_MAX);
        return false;
    }
    auto rService = context->getModel().getReservationService();
    if (!rService) {
        AO_DBG_ERR("Could not access reservations. Ignore");
        return false;
    }
    if (auto reservation = rService->getReservation(connectorId, idTag)) {
        return !reservation->matches(idTag);
    }

    return false; //no reservation -> nothing blocked
}

#if defined(AO_CUSTOM_UPDATER) || defined(AO_CUSTOM_WS)
FirmwareService *getFirmwareService() {
    auto& model = context->getModel();
    return model.getFirmwareService();
}
#endif

#if defined(AO_CUSTOM_DIAGNOSTICS) || defined(AO_CUSTOM_WS)
DiagnosticsService *getDiagnosticsService() {
    auto& model = context->getModel();
    return model.getDiagnosticsService();
}
#endif

Context *getOcppContext() {
    if (!context) {
        AO_DBG_ERR("OCPP uninitialized"); //please call OCPP_initialize before
        return nullptr;
    }

    return context;
}

void setOnReceiveRequest(const char *operationType, OnReceiveReqListener onReceiveReq) {
    if (!context) {
        AO_DBG_ERR("OCPP uninitialized"); //please call OCPP_initialize before
        return;
    }
    if (!operationType) {
        AO_DBG_ERR("invalid args");
        return;
    }
    context->getOperationRegistry().setOnRequest(operationType, onReceiveReq);
}

void setOnSendConf(const char *operationType, OnSendConfListener onSendConf) {
    if (!context) {
        AO_DBG_ERR("OCPP uninitialized"); //please call OCPP_initialize before
        return;
    }
    if (!operationType) {
        AO_DBG_ERR("invalid args");
        return;
    }
    context->getOperationRegistry().setOnResponse(operationType, onSendConf);
}

void sendCustomRequest(const char *operationType,
            std::function<std::unique_ptr<DynamicJsonDocument> ()> fn_createReq,
            std::function<void (JsonObject)> fn_processConf) {

    if (!context) {
        AO_DBG_ERR("OCPP uninitialized"); //please call OCPP_initialize before
        return;
    }
    if (!operationType || !fn_createReq || !fn_processConf) {
        AO_DBG_ERR("invalid args");
        return;
    }

    auto request = makeRequest(new CustomOperation(operationType, fn_createReq, fn_processConf));
    context->initiateRequest(std::move(request));
}

void setCustomRequestHandler(const char *operationType,
            std::function<void (JsonObject)> fn_processReq,
            std::function<std::unique_ptr<DynamicJsonDocument> ()> fn_createConf) {

    if (!context) {
        AO_DBG_ERR("OCPP uninitialized"); //please call OCPP_initialize before
        return;
    }
    if (!operationType || !fn_processReq || !fn_createConf) {
        AO_DBG_ERR("invalid args");
        return;
    }

    std::string captureOpType = operationType;

    context->getOperationRegistry().registerOperation(operationType, [captureOpType, fn_processReq, fn_createConf] () {
        return new CustomOperation(captureOpType.c_str(), fn_processReq, fn_createConf);
    });
}

void authorize(const char *idTag, OnReceiveConfListener onConf, OnAbortListener onAbort, OnTimeoutListener onTimeout, OnReceiveErrorListener onError, unsigned int timeout) {
    if (!context) {
        AO_DBG_ERR("OCPP uninitialized"); //please call OCPP_initialize before
        return;
    }
    if (!idTag || strnlen(idTag, IDTAG_LEN_MAX + 2) > IDTAG_LEN_MAX) {
        AO_DBG_ERR("idTag format violation. Expect c-style string with at most %u characters", IDTAG_LEN_MAX);
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
        AO_DBG_ERR("OCPP uninitialized"); //please call OCPP_initialize before
        return false;
    }
    if (!idTag || strnlen(idTag, IDTAG_LEN_MAX + 2) > IDTAG_LEN_MAX) {
        AO_DBG_ERR("idTag format violation. Expect c-style string with at most %u characters", IDTAG_LEN_MAX);
        return false;
    }
    auto connector = context->getModel().getConnector(OCPP_ID_OF_CONNECTOR);
    if (!connector) {
        AO_DBG_ERR("Could not find connector. Ignore");
        return false;
    }
    auto transaction = connector->getTransaction();
    if (transaction) {
        if (transaction->getStartSync().isRequested()) {
            AO_DBG_ERR("Transaction already in progress. Must call stopTransaction()");
            return false;
        }
        transaction->setIdTag(idTag);
    } else {
        beginTransaction_authorized(idTag); //request new transaction object
        transaction = connector->getTransaction();
        if (!transaction) {
            AO_DBG_WARN("Transaction queue full");
            return false;
        }
    }

    if (auto mService = context->getModel().getMeteringService()) {
        auto meterStart = mService->readTxEnergyMeter(transaction->getConnectorId(), ReadingContext::TransactionBegin);
        if (meterStart && *meterStart) {
            transaction->setMeterStart(meterStart->toInteger());
        } else {
            AO_DBG_ERR("MeterStart undefined");
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
        AO_DBG_ERR("OCPP uninitialized"); //please call OCPP_initialize before
        return false;
    }
    auto connector = context->getModel().getConnector(OCPP_ID_OF_CONNECTOR);
    if (!connector) {
        AO_DBG_ERR("Could not find connector. Ignore");
        return false;
    }

    auto transaction = connector->getTransaction();
    if (!transaction || !transaction->isRunning()) {
        AO_DBG_ERR("No running Tx to stop");
        return false;
    }

    connector->endTransaction("Local");

    const char *idTag = transaction->getIdTag();
    if (idTag) {
        transaction->setStopIdTag(idTag);
    }
    
    transaction->setStopReason("Local");

    if (auto mService = context->getModel().getMeteringService()) {
        auto meterStop = mService->readTxEnergyMeter(transaction->getConnectorId(), ReadingContext::TransactionEnd);
        if (meterStop && *meterStop) {
            transaction->setMeterStop(meterStop->toInteger());
        } else {
            AO_DBG_ERR("MeterStop undefined");
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
