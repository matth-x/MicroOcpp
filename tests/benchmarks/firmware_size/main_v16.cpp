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

    mocpp_initialize(g_loopback, ChargerCredentials());

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
    ocpp_setOnUnlockConnectorInOut([] () {return UnlockConnectorResult_UnlockFailed;});
    isOperative();
    setOnResetNotify([] (bool) {return false;});
    setOnResetExecute([] (bool) {return false;});
    getFirmwareService()->getFirmwareStatus();
    getDiagnosticsService()->getDiagnosticsStatus();
    setCertificateStore(nullptr);
    getOcppContext();

}

void loop() {

    /*
     * Do all OCPP stuff (process WebSocket input, send recorded meter values to Central System, etc.)
     */
    mocpp_loop();

    /*
     * Energize EV plug if OCPP transaction is up and running
     */
    if (ocppPermitsCharge()) {
        //OCPP set up and transaction running. Energize the EV plug here
    } else {
        //No transaction running at the moment. De-energize EV plug
    }

    /*
     * Use NFC reader to start and stop transactions
     */
    
}
