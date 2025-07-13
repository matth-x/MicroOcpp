// matth-x/MicroOcpp
// Copyright Matthias Akstaller 2019 - 2025
// MIT License

#include <MicroOcpp.h>

/*
 * Dummy invokations of all functions in MicroOcpp.h to avoid link-time optimization. This
 * file is only useful for firmware size analysis. The function invokations should not be
 * taken as an example.
 */

void setup() {

    // Basic lifecycle
    mo_deinitialize();
    mo_initialize();
    mo_setup();
    mo_isInitialized();
    MO_Context *ctx = mo_getApiContext();

#if MO_USE_FILEAPI != MO_CUSTOM_FS
    mo_setFilesystemConfig(MO_FS_OPT_DISABLE);
    mo_setFilesystemConfig2(ctx, MO_FS_OPT_DISABLE, "/tmp");
#endif

#if MO_WS_USE == MO_WS_ARDUINO
    mo_setWebsocketUrl("wss://example.com", "chargeBoxId", "authKey", nullptr);
#endif

    // Connection
    mo_setConnection(nullptr);
    mo_setConnection2(ctx, nullptr);

    // OCPP version
    mo_setOcppVersion(MO_OCPP_V16);
    mo_setOcppVersion2(ctx, MO_OCPP_V16);

    // BootNotification
    mo_setBootNotificationData("Model", "Vendor");
    MO_BootNotificationData bnData;
    mo_bootNotificationData_init(&bnData);
    mo_setBootNotificationData2(ctx, bnData);

    // Inputs/Outputs
    mo_setConnectorPluggedInput([] () {return false;});
    mo_setConnectorPluggedInput2(ctx, 1, [] (unsigned int, void*) {return false;}, nullptr);
    mo_setEnergyMeterInput([] () {return 0;});
    mo_setEnergyMeterInput2(ctx, 1, [] (MO_ReadingContext, unsigned int, void*) {return 0;}, nullptr);
    mo_setPowerMeterInput([] () {return 0.f;});
    mo_setPowerMeterInput2(ctx, 1, [] (MO_ReadingContext, unsigned int, void*) {return 0.f;}, nullptr);
#if MO_ENABLE_SMARTCHARGING
    mo_setSmartChargingPowerOutput([] (float) {});
    mo_setSmartChargingCurrentOutput([] (float) {});
    mo_setSmartChargingOutput(ctx, 1, [] (MO_ChargeRate, unsigned int, void*) {}, true, true, true, true, nullptr);
#endif
    mo_setEvReadyInput([] () {return false;});
    mo_setEvReadyInput2(ctx, 1, [] (unsigned int, void*) {return false;}, nullptr);
    mo_setEvseReadyInput([] () {return false;});
    mo_setEvseReadyInput2(ctx, 1, [] (unsigned int, void*) {return false;}, nullptr);
    mo_addErrorCodeInput([] () {return (const char*)nullptr;});
#if MO_ENABLE_V16
    mo_v16_addErrorDataInput(ctx, 1, [] (unsigned int, void*) {MO_ErrorData e; mo_errorData_init(&e); return e;}, nullptr);
#endif
#if MO_ENABLE_V211
    mo_v201_addFaultedInput(ctx, 1, [] (unsigned int, void*) {return false;}, nullptr);
#endif
    mo_addMeterValueInputInt([] () {return 0;}, nullptr, nullptr, nullptr, nullptr);
    mo_addMeterValueInputInt2(ctx, 1, [] (MO_ReadingContext, unsigned int, void*) {return 0;}, nullptr, nullptr, nullptr, nullptr, nullptr);
    mo_addMeterValueInputFloat([] () {return 0.f;}, nullptr, nullptr, nullptr, nullptr);
    mo_addMeterValueInputFloat2(ctx, 1, [] (MO_ReadingContext, unsigned int, void*) {return 0.f;}, nullptr, nullptr, nullptr, nullptr, nullptr);
    mo_addMeterValueInputString([] (char *buf, size_t size) {if (size > 0) {buf[0] = '\0'; return 0;} return 0;}, nullptr, nullptr, nullptr, nullptr);

    // Transaction management
    mo_beginTransaction("");
    mo_beginTransaction2(ctx, 1, "");
    mo_beginTransaction_authorized("", "");
    mo_beginTransaction_authorized2(ctx, 1, "", "");
#if MO_ENABLE_V201
    mo_authorizeTransaction("");
    mo_authorizeTransaction2(ctx, 1, "", MO_IdTokenType_ISO14443);
    mo_authorizeTransaction3(ctx, 1, "", MO_IdTokenType_ISO14443, false, "");
#endif
    mo_endTransaction(nullptr, nullptr);
    mo_endTransaction2(ctx, 1, nullptr, nullptr);
    mo_endTransaction_authorized(nullptr, nullptr);
    mo_endTransaction_authorized2(ctx, 1, nullptr, nullptr);
#if MO_ENABLE_V201
    mo_deauthorizeTransaction(nullptr);
    mo_deauthorizeTransaction2(ctx, 1, nullptr, MO_IdTokenType_ISO14443);
    mo_deauthorizeTransaction3(ctx, 1, nullptr, MO_IdTokenType_ISO14443, false, nullptr);
    mo_abortTransaction(MO_TxStoppedReason_Other, MO_TxEventTriggerReason_AbnormalCondition);
    mo_abortTransaction2(ctx, 1, MO_TxStoppedReason_Other, MO_TxEventTriggerReason_AbnormalCondition);
#endif

    // Status and info
    mo_ocppPermitsCharge();
    mo_ocppPermitsCharge2(ctx, 1);
    mo_isTransactionActive();
    mo_isTransactionActive2(ctx, 1);
    mo_isTransactionRunning();
    mo_isTransactionRunning2(ctx, 1);
    mo_getTransactionIdTag();
    mo_getTransactionIdTag2(ctx, 1);
#if MO_ENABLE_V201
    mo_getTransactionId();
    mo_getTransactionId2(ctx, 1);
#endif
#if MO_ENABLE_V16
    mo_v16_getTransactionId();
    mo_v16_getTransactionId2(ctx, 1);
#endif
    mo_getChargePointStatus();
    mo_getChargePointStatus2(ctx, 1);

    mo_setOnUnlockConnector([] () {return MO_UnlockConnectorResult_UnlockFailed;});

    mo_isOperative();
#if MO_ENABLE_V16
    mo_v16_setOnResetNotify([] (bool) {return false;});
#endif
    mo_setOnResetExecute([] () {});

    mo_sendRequest(ctx, "", "", [] (const char*,void*) {}, [] (void*) {}, nullptr);
    mo_setRequestHandler(ctx, "", [] (const char*,const char*,void**,void*) {}, [] (const char*,char*,size_t,void*,void*) {return 0;}, [] (const char*,void*,void*) {}, nullptr);
    mo_setOnReceiveRequest(ctx, "", [] (const char*,const char*,void*) {}, nullptr);
    mo_setOnSendConf(ctx, "", [] (const char*, const char*, void*) {}, nullptr);

    mo_getContext();

}

void loop() {
    mo_loop();
}
