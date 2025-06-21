// matth-x/MicroOcpp
// Copyright Matthias Akstaller 2019 - 2024
// MIT License

#include <MicroOcpp.h>

void setup() {

    mo_deinitialize();

    mo_initialize();

    mo_setup();

    mo_beginTransaction("");
    mo_beginTransaction_authorized("","");
    mo_endTransaction("","");
    mo_endTransaction_authorized("","");
    mo_isTransactionActive();
    mo_isTransactionRunning();
    mo_getTransactionIdTag();
    mo_v16_getTransactionId();
    mo_v201_getTransactionId();
    mo_ocppPermitsCharge();
    mo_getChargePointStatus();
    mo_setConnectorPluggedInput([] () {return false;});
    mo_setEnergyMeterInput([] () {return 0;});
    mo_setPowerMeterInput([] () {return 0.f;});
    mo_setSmartChargingPowerOutput([] (float) {});
    mo_setSmartChargingCurrentOutput([] (float) {});
    mo_setEvReadyInput([] () {return false;});
    mo_setEvseReadyInput([] () {return false;});
    mo_addErrorCodeInput([] () {return (const char*)nullptr;});
    mo_v16_addErrorDataInput(mo_getApiContext(), 0, [] (unsigned int, void*) {return MO_ErrorData();}, NULL);
    mo_addMeterValueInputFloat([] () {return 0.f;}, NULL, NULL, NULL, NULL);
    mo_setOccupiedInput([] () {return false;});
    mo_setStartTxReadyInput([] () {return false;});
    mo_setStopTxReadyInput([] () {return false;});
    mo_setTxNotificationOutput([] (MO_TxNotification) {});

#if MO_ENABLE_CONNECTOR_LOCK
    mo_setOnUnlockConnector([] () {return MO_UnlockConnectorResult_UnlockFailed;});
#endif

    mo_isOperative();
    mo_v16_setOnResetNotify([] (bool) {return false;});
    mo_setOnResetExecute([] () {});

    mo_getContext();

}

void loop() {
    mo_loop();
}
