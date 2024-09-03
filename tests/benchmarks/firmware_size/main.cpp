// matth-x/MicroOcpp
// Copyright Matthias Akstaller 2019 - 2024
// MIT License

#include <MicroOcpp.h>
#include <MicroOcpp_c.h>
#include <MicroOcpp/Model/FirmwareManagement/FirmwareService.h>
#include <MicroOcpp/Model/Diagnostics/DiagnosticsService.h>

MicroOcpp::LoopbackConnection g_loopback;

void setup() {

    ocpp_deinitialize();

#if MO_ENABLE_V201
    mocpp_initialize(g_loopback, ChargerCredentials::v201(),MicroOcpp::makeDefaultFilesystemAdapter(MicroOcpp::FilesystemOpt::Use_Mount_FormatOnFail),true,MicroOcpp::ProtocolVersion(2,0,1));
#else
    mocpp_initialize(g_loopback, ChargerCredentials()
#endif

    ocpp_beginTransaction("");
    ocpp_beginTransaction_authorized("","");
    ocpp_endTransaction("","");
    ocpp_endTransaction_authorized("","");
    ocpp_isTransactionActive();
    ocpp_isTransactionRunning();
    ocpp_getTransactionIdTag();
    ocpp_getTransaction();
    ocpp_ocppPermitsCharge();
    ocpp_getChargePointStatus();
    ocpp_setConnectorPluggedInput([] () {return false;});
    ocpp_setEnergyMeterInput([] () {return 0;});
    ocpp_setPowerMeterInput([] () {return 0.f;});
    ocpp_setSmartChargingPowerOutput([] (float) {});
    ocpp_setSmartChargingCurrentOutput([] (float) {});
    ocpp_setSmartChargingOutput([] (float,float,int) {});
    ocpp_setEvReadyInput([] () {return false;});
    ocpp_setEvseReadyInput([] () {return false;});
    ocpp_addErrorCodeInput([] () {return (const char*)nullptr;});
    addErrorDataInput([] () {return MicroOcpp::ErrorData("");});
    ocpp_addMeterValueInputFloat([] () {return 0.f;},"","","","");
    ocpp_setOccupiedInput([] () {return false;});
    ocpp_setStartTxReadyInput([] () {return false;});
    ocpp_setStopTxReadyInput([] () {return false;});
    ocpp_setTxNotificationOutput([] (OCPP_Transaction*, OCPP_TxNotification) {});

#if MO_ENABLE_CONNECTOR_LOCK
    ocpp_setOnUnlockConnectorInOut([] () {return UnlockConnectorResult_UnlockFailed;});
#endif

    isOperative();
    setOnResetNotify([] (bool) {return false;});
    setOnResetExecute([] (bool) {return false;});
    getFirmwareService()->getFirmwareStatus();
    getDiagnosticsService()->getDiagnosticsStatus();

#if MO_ENABLE_CERT_MGMT
    setCertificateStore(nullptr);
#endif

    getOcppContext();

}

void loop() {
    mocpp_loop();
}
