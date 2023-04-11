// matth-x/ArduinoOcpp
// Copyright Matthias Akstaller 2019 - 2023
// MIT License

#include "ArduinoOcpp.h"

#include <ArduinoOcpp/Core/OcppEngine.h>
#include <ArduinoOcpp/Core/OcppModel.h>
#include <ArduinoOcpp/Tasks/Metering/MeteringService.h>
#include <ArduinoOcpp/Tasks/SmartCharging/SmartChargingService.h>
#include <ArduinoOcpp/Tasks/ChargePointStatus/ChargePointStatusService.h>
#include <ArduinoOcpp/Tasks/Heartbeat/HeartbeatService.h>
#include <ArduinoOcpp/Tasks/FirmwareManagement/FirmwareService.h>
#include <ArduinoOcpp/Tasks/Diagnostics/DiagnosticsService.h>
#include <ArduinoOcpp/Tasks/Transactions/TransactionStore.h>
#include <ArduinoOcpp/Tasks/Authorization/AuthorizationService.h>
#include <ArduinoOcpp/Tasks/Reservation/ReservationService.h>
#include <ArduinoOcpp/Tasks/Boot/BootService.h>
#include <ArduinoOcpp/SimpleOcppOperationFactory.h>
#include <ArduinoOcpp/OperationDeserializer.h>
#include <ArduinoOcpp/Core/Configuration.h>
#include <ArduinoOcpp/Core/FilesystemAdapter.h>

#include <ArduinoOcpp/MessagesV16/Authorize.h>
#include <ArduinoOcpp/MessagesV16/BootNotification.h>
#include <ArduinoOcpp/MessagesV16/StartTransaction.h>
#include <ArduinoOcpp/MessagesV16/StopTransaction.h>

#include <ArduinoOcpp/Debug.h>

namespace ArduinoOcpp {
namespace Facade {

#ifndef AO_CUSTOM_WS
WebSocketsClient *webSocket {nullptr};
OcppSocket *ocppSocket {nullptr};
#endif

OcppEngine *ocppEngine {nullptr};
std::shared_ptr<FilesystemAdapter> filesystem;
FilesystemOpt fileSystemOpt {};
float voltage_eff {230.f};

#ifndef AO_NUMCONNECTORS
#define AO_NUMCONNECTORS 2
#endif

#define OCPP_ID_OF_CP 0

} //end namespace ArduinoOcpp::Facade
} //end namespace ArduinoOcpp

using namespace ArduinoOcpp;
using namespace ArduinoOcpp::Facade;
using namespace ArduinoOcpp::Ocpp16;

#ifndef AO_CUSTOM_WS
void OCPP_initialize(const char *CS_hostname, uint16_t CS_port, const char *CS_url, const char *chargePointModel, const char *chargePointVendor, float V_eff, ArduinoOcpp::FilesystemOpt fsOpt) {
    if (ocppEngine) {
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

    delete ocppSocket;
    ocppSocket = new EspWiFi::OcppClientSocket(webSocket);

    OCPP_initialize(*ocppSocket, ChargerCredentials(chargePointModel, chargePointVendor), V_eff, fsOpt);
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

void OCPP_initialize(OcppSocket& ocppSocket, const char *bootNotificationCredentials, float V_eff, ArduinoOcpp::FilesystemOpt fsOpt) {
    if (ocppEngine) {
        AO_DBG_WARN("Can't be called two times. To change the credentials, either restart ESP, or call OCPP_deinitialize() before");
        return;
    }

    voltage_eff = V_eff;
    fileSystemOpt = fsOpt;

#ifndef AO_DEACTIVATE_FLASH
    filesystem = makeDefaultFilesystemAdapter(fileSystemOpt);
#endif
    AO_DBG_DEBUG("filesystem %s", filesystem ? "loaded" : "error");
    
    configuration_init(filesystem); //call before each other library call

    ocppEngine = new OcppEngine(ocppSocket, Clocks::DEFAULT_CLOCK, filesystem);
    auto& model = ocppEngine->getOcppModel();

    model.setTransactionStore(std::unique_ptr<TransactionStore>(
        new TransactionStore(AO_NUMCONNECTORS, filesystem)));
    model.setBootService(std::unique_ptr<BootService>(
        new BootService(*ocppEngine)));
    model.setChargePointStatusService(std::unique_ptr<ChargePointStatusService>(
        new ChargePointStatusService(*ocppEngine, AO_NUMCONNECTORS)));
    model.setHeartbeatService(std::unique_ptr<HeartbeatService>(
        new HeartbeatService(*ocppEngine)));
    model.setAuthorizationService(std::unique_ptr<AuthorizationService>(
        new AuthorizationService(*ocppEngine, filesystem)));
    model.setReservationService(std::unique_ptr<ReservationService>(
        new ReservationService(*ocppEngine, AO_NUMCONNECTORS)));
    
#if !defined(AO_CUSTOM_UPDATER) && !defined(AO_CUSTOM_WS)
    model.setFirmwareService(std::unique_ptr<FirmwareService>(
        EspWiFi::makeFirmwareService(*ocppEngine, "1234578901"))); //instantiate FW service + ESP installation routine
#else
    model.setFirmwareService(std::unique_ptr<FirmwareService>(
        new FirmwareService(*ocppEngine))); //only instantiate FW service
#endif

#if !defined(AO_CUSTOM_DIAGNOSTICS) && !defined(AO_CUSTOM_WS)
    model.setDiagnosticsService(std::unique_ptr<DiagnosticsService>(
        EspWiFi::makeDiagnosticsService(*ocppEngine))); //will only return "Rejected" because client needs to implement logging
#else
    model.setDiagnosticsService(std::unique_ptr<DiagnosticsService>(
        new DiagnosticsService(*ocppEngine)));
#endif

#if AO_PLATFORM == AO_PLATFORM_ARDUINO && (defined(ESP32) || defined(ESP8266))
    if (!model.getChargePointStatusService()->getExecuteReset())
        model.getChargePointStatusService()->setExecuteReset(makeDefaultResetFn());
#endif

    model.getBootService()->setChargePointCredentials(bootNotificationCredentials);
}

void OCPP_deinitialize() {
    
    delete ocppEngine;
    ocppEngine = nullptr;

#ifndef AO_CUSTOM_WS
    delete ocppSocket;
    ocppSocket = nullptr;
    delete webSocket;
    webSocket = nullptr;
#endif

    fileSystemOpt = FilesystemOpt();
    voltage_eff = 230.f;
}

void OCPP_loop() {
    if (!ocppEngine) {
        AO_DBG_WARN("Please call OCPP_initialize before");
        return;
    }

    ocppEngine->loop();
}

void bootNotification(const char *chargePointModel, const char *chargePointVendor, OnReceiveConfListener onConf, OnAbortListener onAbort, OnTimeoutListener onTimeout, OnReceiveErrorListener onError, unsigned int timeout) {
    if (!ocppEngine) {
        AO_DBG_ERR("OCPP uninitialized"); //please call OCPP_initialize before
        return;
    }
    
    auto credentials = std::unique_ptr<DynamicJsonDocument>(new DynamicJsonDocument(
        JSON_OBJECT_SIZE(2) + strlen(chargePointModel) + strlen(chargePointVendor) + 2));
    (*credentials)["chargePointModel"] = (char*) chargePointModel;
    (*credentials)["chargePointVendor"] = (char*) chargePointVendor;

    bootNotification(std::move(credentials), onConf, onAbort, onTimeout, onError, timeout);
}

void bootNotification(std::unique_ptr<DynamicJsonDocument> payload, OnReceiveConfListener onConf, OnAbortListener onAbort, OnTimeoutListener onTimeout, OnReceiveErrorListener onError, unsigned int timeout) {
    if (!ocppEngine) {
        AO_DBG_ERR("OCPP uninitialized"); //please call OCPP_initialize before
        return;
    }

    //store credentials for later TriggerMessage
    if (auto bootService = ocppEngine->getOcppModel().getBootService()) {
        bootService->setChargePointCredentials(payload->as<JsonObject>());
    }

    auto bootNotification = makeOcppOperation(
        new BootNotification(ocppEngine->getOcppModel(), std::move(payload)));
    if (onConf)
        bootNotification->setOnReceiveConfListener(onConf);
    if (onAbort)
        bootNotification->setOnAbortListener(onAbort);
    if (onTimeout)
        bootNotification->setOnTimeoutListener(onTimeout);
    if (onError)
        bootNotification->setOnReceiveErrorListener(onError);
    if (timeout)
        bootNotification->setTimeout(timeout);
    else
        bootNotification->setTimeout(0);
    ocppEngine->initiateOperation(std::move(bootNotification));
}

void authorize(const char *idTag, OnReceiveConfListener onConf, OnAbortListener onAbort, OnTimeoutListener onTimeout, OnReceiveErrorListener onError, unsigned int timeout) {
    if (!ocppEngine) {
        AO_DBG_ERR("OCPP uninitialized"); //please call OCPP_initialize before
        return;
    }
    if (!idTag || strnlen(idTag, IDTAG_LEN_MAX + 2) > IDTAG_LEN_MAX) {
        AO_DBG_ERR("idTag format violation. Expect c-style string with at most %u characters", IDTAG_LEN_MAX);
        return;
    }
    auto authorize = makeOcppOperation(
        new Authorize(ocppEngine->getOcppModel(), idTag));
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
    ocppEngine->initiateOperation(std::move(authorize));
}

bool beginTransaction(const char *idTag, unsigned int connectorId) {
    if (!ocppEngine) {
        AO_DBG_ERR("OCPP uninitialized"); //please call OCPP_initialize before
        return false;
    }
    if (!idTag || strnlen(idTag, IDTAG_LEN_MAX + 2) > IDTAG_LEN_MAX) {
        AO_DBG_ERR("idTag format violation. Expect c-style string with at most %u characters", IDTAG_LEN_MAX);
        return false;
    }
    auto connector = ocppEngine->getOcppModel().getConnectorStatus(connectorId);
    if (!connector) {
        AO_DBG_ERR("Could not find connector. Ignore");
        return false;
    }
    connector->beginSession(idTag);
    return true;
}

bool endTransaction(const char *reason, unsigned int connectorId) {
    if (!ocppEngine) {
        AO_DBG_ERR("OCPP uninitialized"); //please call OCPP_initialize before
        return false;
    }
    auto connector = ocppEngine->getOcppModel().getConnectorStatus(connectorId);
    if (!connector) {
        AO_DBG_ERR("Could not find connector. Ignore");
        return false;
    }
    bool res = connector->getSessionIdTag();
    connector->endSession(reason);
    return res;
}

bool isTransactionRunning(unsigned int connectorId) {
    if (!ocppEngine) {
        AO_DBG_ERR("OCPP uninitialized"); //please call OCPP_initialize before
        return false;
    }
    auto connector = ocppEngine->getOcppModel().getConnectorStatus(connectorId);
    if (!connector) {
        AO_DBG_ERR("Could not find connector. Ignore");
        return false;
    }
    return connector->isTransactionRunning();
}

bool ocppPermitsCharge(unsigned int connectorId) {
    if (!ocppEngine) {
        AO_DBG_WARN("Please call OCPP_initialize before");
        return false;
    }
    auto connector = ocppEngine->getOcppModel().getConnectorStatus(connectorId);
    if (!connector) {
        AO_DBG_ERR("Could not find connector. Ignore");
        return false;
    }
    return connector->ocppPermitsCharge();
}

void setConnectorPluggedInput(std::function<bool()> pluggedInput, unsigned int connectorId) {
    if (!ocppEngine) {
        AO_DBG_ERR("OCPP uninitialized"); //please call OCPP_initialize before
        return;
    }
    auto connector = ocppEngine->getOcppModel().getConnectorStatus(connectorId);
    if (!connector) {
        AO_DBG_ERR("Could not find connector. Ignore");
        return;
    }
    connector->setConnectorPluggedSampler(pluggedInput);

    if (pluggedInput) {
        AO_DBG_INFO("Added ConnectorPluggedSampler. Transaction-management is in auto mode now");
    } else {
        AO_DBG_INFO("Reset ConnectorPluggedSampler. Transaction-management is in manual mode now");
    }
}

void setEnergyMeterInput(std::function<float()> energyInput, unsigned int connectorId) {
    if (!ocppEngine) {
        AO_DBG_ERR("OCPP uninitialized"); //please call OCPP_initialize before
        return;
    }
    auto& model = ocppEngine->getOcppModel();
    if (!model.getMeteringService()) {
        model.setMeteringSerivce(std::unique_ptr<MeteringService>(
            new MeteringService(*ocppEngine, AO_NUMCONNECTORS, filesystem)));
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
    model.getMeteringService()->setEnergySampler(connectorId, energyInput);
}

void setPowerMeterInput(std::function<float()> powerInput, unsigned int connectorId) {
    if (!ocppEngine) {
        AO_DBG_ERR("OCPP uninitialized"); //please call OCPP_initialize before
        return;
    }

    auto& model = ocppEngine->getOcppModel();
    if (!model.getMeteringService()) {
        model.setMeteringSerivce(std::unique_ptr<MeteringService>(
            new MeteringService(*ocppEngine, AO_NUMCONNECTORS, filesystem)));
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
    model.getMeteringService()->setPowerSampler(connectorId, powerInput);
}

void setSmartChargingOutput(std::function<void(float)> chargingLimitOutput, unsigned int connectorId) {
    if (!ocppEngine) {
        AO_DBG_ERR("OCPP uninitialized"); //please call OCPP_initialize before
        return;
    }
    if (connectorId != 1) {
        AO_DBG_WARN("Only one connector supported at the moment for Smart Charging");
        return;
    }
    auto& model = ocppEngine->getOcppModel();
    if (!model.getSmartChargingService()) {
        model.setSmartChargingService(std::unique_ptr<SmartChargingService>(
            new SmartChargingService(*ocppEngine, 11000.0f, voltage_eff, AO_NUMCONNECTORS, fileSystemOpt))); //default charging limit: 11kW
    }
    model.getSmartChargingService()->setOnLimitChange(chargingLimitOutput);
}

void setEvReadyInput(std::function<bool()> evReadyInput, unsigned int connectorId) {
    if (!ocppEngine) {
        AO_DBG_ERR("OCPP uninitialized"); //please call OCPP_initialize before
        return;
    }
    auto connector = ocppEngine->getOcppModel().getConnectorStatus(connectorId);
    if (!connector) {
        AO_DBG_ERR("Could not find connector. Ignore");
        return;
    }
    connector->setEvRequestsEnergySampler(evReadyInput);
}

void setEvseReadyInput(std::function<bool()> evseReadyInput, unsigned int connectorId) {
    if (!ocppEngine) {
        AO_DBG_ERR("OCPP uninitialized"); //please call OCPP_initialize before
        return;
    }
    auto connector = ocppEngine->getOcppModel().getConnectorStatus(connectorId);
    if (!connector) {
        AO_DBG_ERR("Could not find connector. Ignore");
        return;
    }
    connector->setConnectorEnergizedSampler(evseReadyInput);
}

void addErrorCodeInput(std::function<const char *()> errorCodeInput, unsigned int connectorId) {
    if (!ocppEngine) {
        AO_DBG_ERR("OCPP uninitialized"); //please call OCPP_initialize before
        return;
    }
    auto connector = ocppEngine->getOcppModel().getConnectorStatus(connectorId);
    if (!connector) {
        AO_DBG_ERR("Could not find connector. Ignore");
        return;
    }
    connector->addConnectorErrorCodeSampler(errorCodeInput);
}

void addMeterValueInput(std::function<int32_t ()> valueInput, const char *measurand, const char *unit, const char *location, const char *phase, unsigned int connectorId) {
    if (!ocppEngine) {
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

    auto valueSampler = std::unique_ptr<ArduinoOcpp::SampledValueSamplerConcrete<int32_t, ArduinoOcpp::SampledValueDeSerializer<int32_t>>>(
                                    new ArduinoOcpp::SampledValueSamplerConcrete<int32_t, ArduinoOcpp::SampledValueDeSerializer<int32_t>>(
                properties,
                [valueInput] (ArduinoOcpp::ReadingContext) -> int32_t {return valueInput();}));
    addMeterValueInput(std::move(valueSampler), connectorId);
}

void addMeterValueInput(std::unique_ptr<SampledValueSampler> valueInput, unsigned int connectorId) {
    if (!ocppEngine) {
        AO_DBG_ERR("OCPP uninitialized"); //please call OCPP_initialize before
        return;
    }
    auto& model = ocppEngine->getOcppModel();
    if (!model.getMeteringService()) {
        model.setMeteringSerivce(std::unique_ptr<MeteringService>(
            new MeteringService(*ocppEngine, AO_NUMCONNECTORS, filesystem)));
    }
    model.getMeteringService()->addMeterValueSampler(connectorId, std::move(valueInput));
}

void setOnResetNotify(std::function<bool(bool)> onResetNotify) {
    if (!ocppEngine) {
        AO_DBG_ERR("OCPP uninitialized"); //please call OCPP_initialize before
        return;
    }

    if (auto csService = ocppEngine->getOcppModel().getChargePointStatusService()) {
        csService->setPreReset(onResetNotify);
    }
}

void setOnResetExecute(std::function<void(bool)> onResetExecute) {
    if (!ocppEngine) {
        AO_DBG_ERR("OCPP uninitialized"); //please call OCPP_initialize before
        return;
    }

    if (auto csService = ocppEngine->getOcppModel().getChargePointStatusService()) {
        csService->setExecuteReset(onResetExecute);
    }
}

void setOnUnlockConnectorInOut(std::function<PollResult<bool>()> onUnlockConnectorInOut, unsigned int connectorId) {
    if (!ocppEngine) {
        AO_DBG_ERR("OCPP uninitialized"); //please call OCPP_initialize before
        return;
    }
    auto connector = ocppEngine->getOcppModel().getConnectorStatus(connectorId);
    if (!connector) {
        AO_DBG_ERR("Could not find connector. Ignore");
        return;
    }
    connector->setOnUnlockConnector(onUnlockConnectorInOut);
}

void setConnectorLockInOut(std::function<ArduinoOcpp::TxEnableState(ArduinoOcpp::TxTrigger)> lockConnectorInOut, unsigned int connectorId) {
    if (!ocppEngine) {
        AO_DBG_ERR("OCPP uninitialized"); //please call OCPP_initialize before
        return;
    }
    auto connector = ocppEngine->getOcppModel().getConnectorStatus(connectorId);
    if (!connector) {
        AO_DBG_ERR("Could not find connector. Ignore");
        return;
    }
    connector->setConnectorLock(lockConnectorInOut);
}

void setTxBasedMeterInOut(std::function<ArduinoOcpp::TxEnableState(ArduinoOcpp::TxTrigger)> txMeterInOut, unsigned int connectorId) {
    if (!ocppEngine) {
        AO_DBG_ERR("OCPP uninitialized"); //please call OCPP_initialize before
        return;
    }
    auto connector = ocppEngine->getOcppModel().getConnectorStatus(connectorId);
    if (!connector) {
        AO_DBG_ERR("Could not find connector. Ignore");
        return;
    }
    connector->setTxBasedMeterUpdate(txMeterInOut);
}

bool isOperative(unsigned int connectorId) {
    if (!ocppEngine) {
        AO_DBG_WARN("Please call OCPP_initialize before");
        return true; //assume "true" as default state
    }
    auto& model = ocppEngine->getOcppModel();
    auto chargePoint = model.getConnectorStatus(OCPP_ID_OF_CP);
    auto connector = model.getConnectorStatus(connectorId);
    if (!chargePoint || !connector) {
        AO_DBG_ERR("Could not find connector. Ignore");
        return true; //assume "true" as default state
    }
    return (chargePoint->getAvailability() != AVAILABILITY_INOPERATIVE)
       &&  (connector->getAvailability() != AVAILABILITY_INOPERATIVE);
}

int getTransactionId(unsigned int connectorId) {
    if (!ocppEngine) {
        AO_DBG_WARN("Please call OCPP_initialize before");
        return -1;
    }
    auto connector = ocppEngine->getOcppModel().getConnectorStatus(connectorId);
    if (!connector) {
        AO_DBG_ERR("Could not find connector. Ignore");
        return -1;
    }
    return connector->getTransactionId();
}

const char *getTransactionIdTag(unsigned int connectorId) {
    if (!ocppEngine) {
        AO_DBG_WARN("Please call OCPP_initialize before");
        return nullptr;
    }
    auto connector = ocppEngine->getOcppModel().getConnectorStatus(connectorId);
    if (!connector) {
        AO_DBG_ERR("Could not find connector. Ignore");
        return nullptr;
    }
    return connector->getSessionIdTag();
}

bool isBlockedByReservation(const char *idTag, unsigned int connectorId) {
    if (!ocppEngine) {
        AO_DBG_WARN("Please call OCPP_initialize before");
        return false;
    }
    if (!idTag || strnlen(idTag, IDTAG_LEN_MAX + 2) > IDTAG_LEN_MAX) {
        AO_DBG_ERR("idTag format violation. Expect c-style string with at most %u characters", IDTAG_LEN_MAX);
        return false;
    }
    auto rService = ocppEngine->getOcppModel().getReservationService();
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
ArduinoOcpp::FirmwareService *getFirmwareService() {
    auto& model = ocppEngine->getOcppModel();
    return model.getFirmwareService();
}
#endif

#if defined(AO_CUSTOM_DIAGNOSTICS) || defined(AO_CUSTOM_WS)
ArduinoOcpp::DiagnosticsService *getDiagnosticsService() {
    auto& model = ocppEngine->getOcppModel();
    return model.getDiagnosticsService();
}
#endif

OcppEngine *getOcppEngine() {
    if (!ocppEngine) {
        AO_DBG_ERR("OCPP uninitialized"); //please call OCPP_initialize before
        return nullptr;
    }

    return ocppEngine;
}

void setOnSetChargingProfileRequest(OnReceiveReqListener onReceiveReq) {
    if (!ocppEngine) {
        AO_DBG_ERR("OCPP uninitialized"); //please call OCPP_initialize before
        return;
    }
    ocppEngine->getOperationDeserializer().setOnRequest("SetChargingProfile", onReceiveReq);
}

void setOnRemoteStartTransactionSendConf(OnSendConfListener onSendConf) {
    if (!ocppEngine) {
        AO_DBG_ERR("OCPP uninitialized"); //please call OCPP_initialize before
        return;
    }
    ocppEngine->getOperationDeserializer().setOnResponse("RemoteStartTransaction", onSendConf);
}

void setOnRemoteStopTransactionReceiveReq(OnReceiveReqListener onReceiveReq) {
    if (!ocppEngine) {
        AO_DBG_ERR("OCPP uninitialized"); //please call OCPP_initialize before
        return;
    }
    ocppEngine->getOperationDeserializer().setOnRequest("RemoteStopTransaction", onReceiveReq);
}

void setOnRemoteStopTransactionSendConf(OnSendConfListener onSendConf) {
    if (!ocppEngine) {
        AO_DBG_ERR("OCPP uninitialized"); //please call OCPP_initialize before
        return;
    }
    ocppEngine->getOperationDeserializer().setOnResponse("RemoteStopTransaction", onSendConf);
}

void setOnResetSendConf(OnSendConfListener onSendConf) {
    if (!ocppEngine) {
        AO_DBG_ERR("OCPP uninitialized"); //please call OCPP_initialize before
        return;
    }
    ocppEngine->getOperationDeserializer().setOnResponse("Reset", onSendConf);
}

void setOnResetRequest(OnReceiveReqListener onReceiveReq) {
    if (!ocppEngine) {
        AO_DBG_ERR("OCPP uninitialized"); //please call OCPP_initialize before
        return;
    }
    ocppEngine->getOperationDeserializer().setOnRequest("Reset", onReceiveReq);
}

#define OCPP_ID_OF_CONNECTOR 1

bool startTransaction(const char *idTag, OnReceiveConfListener onConf, OnAbortListener onAbort, OnTimeoutListener onTimeout, OnReceiveErrorListener onError, unsigned int timeout) {
    if (!ocppEngine) {
        AO_DBG_ERR("OCPP uninitialized"); //please call OCPP_initialize before
        return false;
    }
    if (!idTag || strnlen(idTag, IDTAG_LEN_MAX + 2) > IDTAG_LEN_MAX) {
        AO_DBG_ERR("idTag format violation. Expect c-style string with at most %u characters", IDTAG_LEN_MAX);
        return false;
    }
    auto connector = ocppEngine->getOcppModel().getConnectorStatus(OCPP_ID_OF_CONNECTOR);
    if (!connector) {
        AO_DBG_ERR("Could not find connector. Ignore");
        return false;
    }
    auto transaction = connector->getTransaction();
    if (transaction) {
        if (transaction->getStartRpcSync().isRequested()) {
            AO_DBG_ERR("Transaction already in progress. Must call stopTransaction()");
            return false;
        }
        transaction->setIdTag(idTag);
    } else {
        beginTransaction(idTag); //request new transaction object
        transaction = connector->getTransaction();
        if (!transaction) {
            AO_DBG_WARN("Transaction queue full");
            return false;
        }
    }

    if (auto mService = ocppEngine->getOcppModel().getMeteringService()) {
        auto meterStart = mService->readTxEnergyMeter(transaction->getConnectorId(), ReadingContext::TransactionBegin);
        if (meterStart && *meterStart) {
            transaction->setMeterStart(meterStart->toInteger());
        } else {
            AO_DBG_ERR("MeterStart undefined");
        }
    }

    transaction->setStartTimestamp(ocppEngine->getOcppModel().getOcppTime().getOcppTimestampNow());

    transaction->commit();
    
    auto startTransaction = makeOcppOperation(
        new StartTransaction(ocppEngine->getOcppModel(), transaction));
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
    ocppEngine->initiateOperation(std::move(startTransaction));

    return true;
}

bool stopTransaction(OnReceiveConfListener onConf, OnAbortListener onAbort, OnTimeoutListener onTimeout, OnReceiveErrorListener onError, unsigned int timeout) {
    if (!ocppEngine) {
        AO_DBG_ERR("OCPP uninitialized"); //please call OCPP_initialize before
        return false;
    }
    auto connector = ocppEngine->getOcppModel().getConnectorStatus(OCPP_ID_OF_CONNECTOR);
    if (!connector) {
        AO_DBG_ERR("Could not find connector. Ignore");
        return false;
    }

    auto transaction = connector->getTransaction();
    if (!transaction || !transaction->isRunning()) {
        AO_DBG_ERR("No running Tx to stop");
        return false;
    }

    connector->endSession("Local");

    const char *idTag = transaction->getIdTag();
    if (idTag) {
        transaction->setStopIdTag(idTag);
    }
    
    transaction->setStopReason("Local");

    if (auto mService = ocppEngine->getOcppModel().getMeteringService()) {
        auto meterStop = mService->readTxEnergyMeter(transaction->getConnectorId(), ReadingContext::TransactionEnd);
        if (meterStop && *meterStop) {
            transaction->setMeterStop(meterStop->toInteger());
        } else {
            AO_DBG_ERR("MeterStop undefined");
        }
    }

    transaction->setStopTimestamp(ocppEngine->getOcppModel().getOcppTime().getOcppTimestampNow());

    transaction->commit();

    auto stopTransaction = makeOcppOperation(
        new StopTransaction(ocppEngine->getOcppModel(), transaction));
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
    ocppEngine->initiateOperation(std::move(stopTransaction));

    return true;
}
