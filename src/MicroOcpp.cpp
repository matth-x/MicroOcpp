// matth-x/MicroOcpp
// Copyright Matthias Akstaller 2019 - 2024
// MIT License

#include "MicroOcpp.h"

#include <MicroOcpp/Context.h>
#include <MicroOcpp/Model/Model.h>
#include <MicroOcpp/Model/Boot/BootService.h>
#include <MicroOcpp/Model/Availability/AvailabilityService.h>
#include <MicroOcpp/Model/Transactions/TransactionService16.h>
#include <MicroOcpp/Model/Transactions/TransactionService201.h>
#include <MicroOcpp/Model/Metering/MeterValue.h>
#include <MicroOcpp/Model/Metering/MeteringService.h>
#include <MicroOcpp/Model/Configuration/ConfigurationService.h>

#include <MicroOcpp/Model/SmartCharging/SmartChargingService.h>
#include <MicroOcpp/Model/Heartbeat/HeartbeatService.h>
#include <MicroOcpp/Model/FirmwareManagement/FirmwareService.h>
#include <MicroOcpp/Model/Diagnostics/DiagnosticsService.h>
#include <MicroOcpp/Model/Transactions/TransactionStore.h>
#include <MicroOcpp/Model/Authorization/AuthorizationService.h>
#include <MicroOcpp/Model/Reservation/ReservationService.h>
#include <MicroOcpp/Model/Reset/ResetService.h>
#include <MicroOcpp/Model/Variables/VariableService.h>
#include <MicroOcpp/Model/Certificates/CertificateService.h>
#include <MicroOcpp/Model/Certificates/CertificateMbedTLS.h>
#include <MicroOcpp/Model/RemoteControl/RemoteControlService.h>
#include <MicroOcpp/Core/Request.h>
#include <MicroOcpp/Core/FilesystemAdapter.h>
#include <MicroOcpp/Core/FilesystemUtils.h>
#include <MicroOcpp/Core/Ftp.h>
#include <MicroOcpp/Core/FtpMbedTLS.h>

#include <MicroOcpp/Operations/Authorize.h>
#include <MicroOcpp/Operations/StartTransaction.h>
#include <MicroOcpp/Operations/StopTransaction.h>
#include <MicroOcpp/Operations/CustomOperation.h>

#include <MicroOcpp/Debug.h>

namespace MO_Facade {

MicroOcpp::Context *g_context = nullptr;

} //namespace MO_Facade

using namespace MO_Facade;

#define EVSE_ID_0 0
#define EVSE_ID_1 1

#define CONFIGS_FN_FACADE "user-config.jsn"

#if MO_ENABLE_HEAP_PROFILER
#ifndef MO_HEAP_PROFILER_EXTERNAL_CONTROL
#define MO_HEAP_PROFILER_EXTERNAL_CONTROL 0 //enable if you want to manually reset the heap profiler (e.g. for keeping stats over multiple MO lifecycles)
#endif
#endif

//Begin the lifecycle of MO. Now, it is possible to set up the library. After configuring MO,
//call mo_setup() to finalize the setup
bool mo_initialize() {
    if (g_context) {
        MO_DBG_INFO("already initialized");
        return true;
    }

    g_context = new MicroOcpp::Context();
    if (!g_context) {
        MO_DBG_ERR("OOM");
        return false;
    }
    return true;
}

//End the lifecycle of MO and free all resources
void mo_deinitialize() {
    delete g_context;
    g_context = nullptr;
}

//Returns if library is initialized
bool mo_isInitialized() {
    return g_context != nullptr;
}

MO_Context *mo_getApiContext() {
    return reinterpret_cast<MO_Context*>(g_context);
}

#if MO_USE_FILEAPI != MO_CUSTOM_FS
//Set if MO can use the filesystem and if it needs to mount it
void mo_setFilesystemConfig(MO_FilesystemOpt opt) {
    mo_setFilesystemConfig2(mo_getApiContext(), opt, MO_FILENAME_PREFIX);
}

void mo_setFilesystemConfig2(MO_Context *ctx, MO_FilesystemOpt opt, const char *pathPrefix) {
    if (!ctx) {
        MO_DBG_ERR("OCPP uninitialized"); //need to call mocpp_initialize before
        return;
    }
    auto context = mo_getContext2(ctx);

    MO_FilesystemConfig filesystemConfig;
    mo_filesystemConfig_init(&filesystemConfig);
    filesystemConfig.opt = opt;
    filesystemConfig.path_prefix = pathPrefix;

    context->setFilesystemConfig(filesystemConfig);
}
#endif // MO_USE_FILEAPI != MO_CUSTOM_FS

#if MO_WS_USE == MO_WS_ARDUINO
//Setup MO with links2004/WebSockets library
bool mo_setWebsocketUrl(const char *backendUrl, const char *chargeBoxId, const char *authorizationKey, const char *CA_cert) {
    if (!g_context) {
        MO_DBG_ERR("OCPP uninitialized"); //need to call mocpp_initialize before
        return false;
    }

    if (!backendUrl) {
        MO_DBG_ERR("invalid args");
        return false;
    }

    MO_ConnectionConfig config;
    mo_connectionConfig_init(&config);
    config.backendUrl = backendUrl;
    config.chargeBoxId = chargeBoxId;
    config.authorizationKey = authorizationKey;
    config.CA_cert = CA_cert;

    g_context->setConnectionConfig(config);
    return true;
}
#endif

#if __cplusplus
// Set a WebSocket Client
void mo_setConnection(MicroOcpp::Connection *connection) {
    mo_setConnection2(mo_getApiContext(), connection);
}

void mo_setConnection2(MO_Context *ctx, MicroOcpp::Connection *connection) {
    if (!ctx) {
        MO_DBG_ERR("OCPP uninitialized"); //need to call mocpp_initialize before
        return;
    }
    auto context = mo_getContext2(ctx);

    context->setConnection(connection);
}
#endif

//Set the OCPP version
void mo_setOcppVersion(int ocppVersion) {
    mo_setOcppVersion2(mo_getApiContext(), ocppVersion);
}

void mo_setOcppVersion2(MO_Context *ctx, int ocppVersion) {
    if (!ctx) {
        MO_DBG_ERR("OCPP uninitialized"); //need to call mocpp_initialize before
        return;
    }
    auto context = mo_getContext2(ctx);

    context->setOcppVersion(ocppVersion);
}

//Set BootNotification fields
bool mo_setBootNotificationData(const char *chargePointModel, const char *chargePointVendor) {
    MO_BootNotificationData bnData;
    mo_bootNotificationData_init(&bnData);
    bnData.chargePointModel = chargePointModel;
    bnData.chargePointVendor = chargePointVendor;

    return mo_setBootNotificationData2(mo_getApiContext(), bnData);
}

bool mo_setBootNotificationData2(MO_Context *ctx, MO_BootNotificationData bnData) {
    if (!ctx) {
        MO_DBG_ERR("OCPP uninitialized"); //need to call mocpp_initialize before
        return false;
    }
    auto context = mo_getContext2(ctx);

    MicroOcpp::BootService *bootService = context->getModelCommon().getBootService();
    if (!bootService) {
        MO_DBG_ERR("OOM");
        return false;
    }

    return bootService->setBootNotificationData(bnData);
}

//Input about if an EV is plugged to this EVSE
void mo_setConnectorPluggedInput(bool (*connectorPlugged)()) {
    //convert and forward callback to `mo_setConnectorPluggedInput2()`
    mo_setConnectorPluggedInput2(mo_getApiContext(), EVSE_ID_1, [] (unsigned int, void *userData) {
        auto connectorPlugged = reinterpret_cast<bool (*)()>(userData);
        return connectorPlugged();
    }, reinterpret_cast<void*>(connectorPlugged));
}

void mo_setConnectorPluggedInput2(MO_Context *ctx, unsigned int evseId, bool (*connectorPlugged2)(unsigned int, void*), void *userData) {
    if (!ctx) {
        MO_DBG_ERR("OCPP uninitialized"); //need to call mocpp_initialize before
        return;
    }
    auto context = mo_getContext2(ctx);

    #if MO_ENABLE_V16
    if (context->getOcppVersion() == MO_OCPP_V16) {
        auto availSvc = context->getModel16().getAvailabilityService();
        auto availSvcEvse = availSvc ? availSvc->getEvse(evseId) : nullptr;
        if (!availSvcEvse) {
            MO_DBG_ERR("init failure");
            return;
        }
        availSvcEvse->setConnectorPluggedInput(connectorPlugged2, userData);

        auto txSvc = context->getModel16().getTransactionService();
        auto txSvcEvse = txSvc ? txSvc->getEvse(evseId) : nullptr;
        if (!txSvcEvse) {
            MO_DBG_ERR("init failure");
            return;
        }
        txSvcEvse->setConnectorPluggedInput(connectorPlugged2, userData);
    }
    #endif
    #if MO_ENABLE_V201
    if (context->getOcppVersion() == MO_OCPP_V201) {
        auto availSvc = context->getModel201().getAvailabilityService();
        auto availSvcEvse = availSvc ? availSvc->getEvse(evseId) : nullptr;
        if (!availSvcEvse) {
            MO_DBG_ERR("init failure");
            return;
        }
        availSvcEvse->setConnectorPluggedInput(connectorPlugged2, userData);

        auto txSvc = context->getModel201().getTransactionService();
        auto txSvcEvse = txSvc ? txSvc->getEvse(evseId) : nullptr;
        if (!txSvcEvse) {
            MO_DBG_ERR("init failure");
            return;
        }
        txSvcEvse->setConnectorPluggedInput(connectorPlugged2, userData);
    }
    #endif
}

//Input of the electricity meter register in Wh
bool mo_setEnergyMeterInput(int32_t (*energyInput)()) {
    MO_MeterInput mInput;
    mo_meterInput_init(&mInput, MO_MeterInputType_Int);
    mInput.getInt = energyInput;
    mInput.measurand = "Energy.Active.Import.Register";
    mInput.unit = "Wh";
    return mo_addMeterValueInput(mo_getApiContext(), EVSE_ID_1, mInput);
}

bool mo_setEnergyMeterInput2(MO_Context *ctx, unsigned int evseId, int32_t (*energyInput2)(MO_ReadingContext, unsigned int, void*), void *userData) {
    MO_MeterInput mInput;
    mo_meterInput_init(&mInput, MO_MeterInputType_IntWithArgs);
    mInput.getInt2 = energyInput2;
    mInput.measurand = "Energy.Active.Import.Register";
    mInput.unit = "Wh";
    mInput.user_data = userData;
    return mo_addMeterValueInput(ctx, evseId, mInput);
}

//Input of the power meter reading in W
bool mo_setPowerMeterInput(float (*powerInput)()) {
    MO_MeterInput mInput;
    mo_meterInput_init(&mInput, MO_MeterInputType_Float);
    mInput.getFloat = powerInput;
    mInput.measurand = "Power.Active.Import";
    mInput.unit = "W";
    return mo_addMeterValueInput(mo_getApiContext(), EVSE_ID_1, mInput);
}

bool mo_setPowerMeterInput2(MO_Context *ctx, unsigned int evseId, float (*powerInput2)(MO_ReadingContext, unsigned int, void*), void *userData) {
    MO_MeterInput mInput;
    mo_meterInput_init(&mInput, MO_MeterInputType_Float);
    mInput.getFloat2 = powerInput2;
    mInput.measurand = "Power.Active.Import";
    mInput.unit = "W";
    mInput.user_data = userData;
    return mo_addMeterValueInput(ctx, evseId, mInput);
}

//Smart Charging Output
bool mo_setSmartChargingPowerOutput(void (*powerLimitOutput)(float)) {
    //convert and forward callback to `mo_setSmartChargingOutput()`
    return mo_setSmartChargingOutput(mo_getApiContext(), EVSE_ID_1, [] (MO_ChargeRate limit, unsigned int, void *userData) {
        auto powerLimitOutput = reinterpret_cast<void (*)(float)>(userData);

        powerLimitOutput(limit.power);
    }, /*powerSupported*/ true, false, false, false, reinterpret_cast<void*>(powerLimitOutput));
}

bool mo_setSmartChargingCurrentOutput(void (*currentLimitOutput)(float)) {
    //convert and forward callback to `mo_setSmartChargingOutput()`
    return mo_setSmartChargingOutput(mo_getApiContext(), EVSE_ID_1, [] (MO_ChargeRate limit, unsigned int, void *userData) {
        auto currentLimitOutput = reinterpret_cast<void (*)(float)>(userData);

        currentLimitOutput(limit.current);
    }, false, /*currentSupported*/ true, false, false, reinterpret_cast<void*>(currentLimitOutput));
}

bool mo_setSmartChargingOutput(MO_Context *ctx, unsigned int evseId, void (*chargingLimitOutput)(MO_ChargeRate, unsigned int, void*), bool powerSupported, bool currentSupported, bool phases3to1Supported, bool phaseSwitchingSupported, void *userData) {
    if (!ctx) {
        MO_DBG_ERR("OCPP uninitialized"); //need to call mocpp_initialize before
        return false;
    }
    auto context = mo_getContext2(ctx);

    MicroOcpp::SmartChargingService *scService = context->getModelCommon().getSmartChargingService();
    if (!scService) {
        MO_DBG_ERR("OOM");
        return false;
    }

    return scService->setSmartChargingOutput(evseId, chargingLimitOutput, powerSupported, currentSupported, phases3to1Supported, phaseSwitchingSupported, userData);
}

//Finalize setup. After this, the library is ready to be used and mo_loop() can be run
bool mo_setup() {
    if (!g_context) {
        MO_DBG_ERR("OCPP uninitialized"); //need to call mocpp_initialize before
        return false;
    }

    return g_context->setup();
}

//Run library routines. To be called in the main loop (e.g. place it inside `loop()`)
void mo_loop() {
    if (!g_context) {
        MO_DBG_ERR("OCPP uninitialized"); //need to call mocpp_initialize before
        return;
    }
    g_context->loop();
}

//Transaction management.
bool mo_beginTransaction(const char *idTag) {
    return mo_beginTransaction2(mo_getApiContext(), EVSE_ID_1, idTag);
}

bool mo_beginTransaction2(MO_Context *ctx, unsigned int evseId, const char *idTag) {
    if (!ctx) {
        MO_DBG_ERR("OCPP uninitialized"); //need to call mocpp_initialize before
        return false;
    }
    auto context = mo_getContext2(ctx);

    bool success = false;

    #if MO_ENABLE_V16
    if (context->getOcppVersion() == MO_OCPP_V16) {
        auto txSvc = context->getModel16().getTransactionService();
        auto txSvcEvse = txSvc ? txSvc->getEvse(evseId) : nullptr;
        if (!txSvcEvse) {
            MO_DBG_ERR("OOM");
            return false;
        }
        success = txSvcEvse->beginTransaction(idTag);
    }
    #endif
    #if MO_ENABLE_V201
    if (context->getOcppVersion() == MO_OCPP_V201) {
        success = mo_authorizeTransaction2(ctx, evseId, idTag, MO_IdTokenType_ISO14443);
    }
    #endif

    return success;
}

//Same as `mo_beginTransaction()`, but skip authorization. `parentIdTag` can be NULL
bool mo_beginTransaction_authorized(const char *idTag, const char *parentIdTag) {
    return mo_beginTransaction_authorized2(mo_getApiContext(), EVSE_ID_1, idTag, parentIdTag);
}

bool mo_beginTransaction_authorized2(MO_Context *ctx, unsigned int evseId, const char *idTag, const char *parentIdTag) {
    if (!ctx) {
        MO_DBG_ERR("OCPP uninitialized"); //need to call mocpp_initialize before
        return false;
    }
    auto context = mo_getContext2(ctx);

    bool success = false;

    #if MO_ENABLE_V16
    if (context->getOcppVersion() == MO_OCPP_V16) {
        auto txSvc = context->getModel16().getTransactionService();
        auto txSvcEvse = txSvc ? txSvc->getEvse(evseId) : nullptr;
        if (!txSvcEvse) {
            MO_DBG_ERR("OOM");
            return false;
        }
        success = txSvcEvse->beginTransaction_authorized(idTag, parentIdTag);
    }
    #endif
    #if MO_ENABLE_V201
    if (context->getOcppVersion() == MO_OCPP_V201) {
        success = mo_authorizeTransaction3(ctx, evseId, idTag, MO_IdTokenType_ISO14443, false, parentIdTag);
    }
    #endif

    return success;
}

#if MO_ENABLE_V201
bool mo_authorizeTransaction(const char *idToken) {
    return mo_authorizeTransaction2(mo_getApiContext(), EVSE_ID_1, idToken, MO_IdTokenType_ISO14443);
}

bool mo_authorizeTransaction2(MO_Context *ctx, unsigned int evseId, const char *idToken, MO_IdTokenType type) {
    if (!ctx) {
        MO_DBG_ERR("OCPP uninitialized"); //need to call mocpp_initialize before
        return false;
    }
    auto context = mo_getContext2(ctx);

    bool success = false;

    #if MO_ENABLE_V16
    if (context->getOcppVersion() == MO_OCPP_V16) {
        success = mo_beginTransaction2(ctx, evseId, idToken);
    }
    #endif
    #if MO_ENABLE_V201
    if (context->getOcppVersion() == MO_OCPP_V201) {
        auto txSvc = context->getModel201().getTransactionService();
        auto txSvcEvse = txSvc ? txSvc->getEvse(evseId) : nullptr;
        if (!txSvcEvse) {
            MO_DBG_ERR("OOM");
            return false;
        }
        success = txSvcEvse->beginAuthorization(MicroOcpp::v201::IdToken(idToken, type), /*validateIdToken*/ true);
    }
    #endif

    return success;
}

bool mo_authorizeTransaction3(MO_Context *ctx, unsigned int evseId, const char *idToken, MO_IdTokenType type, bool validateIdToken, const char *groupIdToken) {
    if (!ctx) {
        MO_DBG_ERR("OCPP uninitialized"); //need to call mocpp_initialize before
        return false;
    }
    auto context = mo_getContext2(ctx);

    bool success = false;

    #if MO_ENABLE_V16
    if (context->getOcppVersion() == MO_OCPP_V16) {
        if (validateIdToken) {
            success = mo_beginTransaction2(ctx, evseId, idToken);
        } else {
            success = mo_beginTransaction_authorized2(ctx, evseId, idToken, nullptr);
        }
    }
    #endif
    #if MO_ENABLE_V201
    if (context->getOcppVersion() == MO_OCPP_V201) {
        auto txSvc = context->getModel201().getTransactionService();
        auto txSvcEvse = txSvc ? txSvc->getEvse(evseId) : nullptr;
        if (!txSvcEvse) {
            MO_DBG_ERR("OOM");
            return false;
        }
        success = txSvcEvse->beginAuthorization(MicroOcpp::v201::IdToken(idToken, type), validateIdToken, groupIdToken);
    }
    #endif

    return success;
}
#endif //MO_ENABLE_V201

//End the transaction process if idTag is authorized
bool mo_endTransaction(const char *idTag, const char *reason) {
    return mo_endTransaction2(mo_getApiContext(), EVSE_ID_1, idTag, reason);
}

bool mo_endTransaction2(MO_Context *ctx, unsigned int evseId, const char *idTag, const char *reason) {
    if (!ctx) {
        MO_DBG_ERR("OCPP uninitialized"); //need to call mocpp_initialize before
        return false;
    }
    auto context = mo_getContext2(ctx);

    bool success = false;

    #if MO_ENABLE_V16
    if (context->getOcppVersion() == MO_OCPP_V16) {
        auto txSvc = context->getModel16().getTransactionService();
        auto txSvcEvse = txSvc ? txSvc->getEvse(evseId) : nullptr;
        if (!txSvcEvse) {
            MO_DBG_ERR("OOM");
            return false;
        }
        success = txSvcEvse->endTransaction(idTag, reason);
    }
    #endif
    #if MO_ENABLE_V201
    if (context->getOcppVersion() == MO_OCPP_V201) {
        if (!reason || !strcmp(reason, "Local")) {
            success = mo_deauthorizeTransaction2(ctx, evseId, idTag, MO_IdTokenType_ISO14443);
        } else if (!strcmp(reason, "PowerLoss")) {
            success = mo_abortTransaction2(ctx, evseId, MO_TxStoppedReason_PowerLoss, MO_TxEventTriggerReason_AbnormalCondition);
        } else if (!strcmp(reason, "Reboot")) {
            success = mo_abortTransaction2(ctx, evseId, MO_TxStoppedReason_Reboot, MO_TxEventTriggerReason_ResetCommand);
        } else if (idTag) {
            success = mo_deauthorizeTransaction2(ctx, evseId, idTag, MO_IdTokenType_ISO14443);
        } else {
            success = mo_abortTransaction2(ctx, evseId, MO_TxStoppedReason_Other, MO_TxEventTriggerReason_AbnormalCondition);
        }
    }
    #endif

    return success;
}

//Same as `endTransaction()`, but skip authorization
bool mo_endTransaction_authorized(const char *idTag, const char *reason) {
    return mo_endTransaction_authorized2(mo_getApiContext(), EVSE_ID_1, idTag, reason);
}

bool mo_endTransaction_authorized2(MO_Context *ctx, unsigned int evseId, const char *idTag, const char *reason) {
    if (!ctx) {
        MO_DBG_ERR("OCPP uninitialized"); //need to call mocpp_initialize before
        return false;
    }
    auto context = mo_getContext2(ctx);

    bool success = false;

    #if MO_ENABLE_V16
    if (context->getOcppVersion() == MO_OCPP_V16) {
        auto txSvc = context->getModel16().getTransactionService();
        auto txSvcEvse = txSvc ? txSvc->getEvse(evseId) : nullptr;
        if (!txSvcEvse) {
            MO_DBG_ERR("OOM");
            return false;
        }
        success = txSvcEvse->endTransaction_authorized(idTag, reason);
    }
    #endif
    #if MO_ENABLE_V201
    if (context->getOcppVersion() == MO_OCPP_V201) {
        if (!reason || !strcmp(reason, "Local")) {
            success = mo_deauthorizeTransaction3(ctx, evseId, idTag, MO_IdTokenType_ISO14443, false, NULL);
        } else if (!strcmp(reason, "PowerLoss")) {
            success = mo_abortTransaction2(ctx, evseId, MO_TxStoppedReason_PowerLoss, MO_TxEventTriggerReason_AbnormalCondition);
        } else if (!strcmp(reason, "Reboot")) {
            success = mo_abortTransaction2(ctx, evseId, MO_TxStoppedReason_Reboot, MO_TxEventTriggerReason_ResetCommand);
        } else if (idTag) {
            success = mo_deauthorizeTransaction3(ctx, evseId, idTag, MO_IdTokenType_ISO14443, false, NULL);
        } else {
            success = mo_abortTransaction2(ctx, evseId, MO_TxStoppedReason_Other, MO_TxEventTriggerReason_AbnormalCondition);
        }
    }
    #endif

    return success;
}

#if MO_ENABLE_V201
//Same as mo_endTransaction
bool mo_deauthorizeTransaction(const char *idToken) {
    return mo_deauthorizeTransaction2(mo_getApiContext(), EVSE_ID_1, idToken, MO_IdTokenType_ISO14443);
}

bool mo_deauthorizeTransaction2(MO_Context *ctx, unsigned int evseId, const char *idToken, MO_IdTokenType type) {
    if (!ctx) {
        MO_DBG_ERR("OCPP uninitialized"); //need to call mocpp_initialize before
        return false;
    }
    auto context = mo_getContext2(ctx);

    bool success = false;

    #if MO_ENABLE_V16
    if (context->getOcppVersion() == MO_OCPP_V16) {
        success = mo_endTransaction2(ctx, evseId, idToken, "Local");
    }
    #endif
    #if MO_ENABLE_V201
    if (context->getOcppVersion() == MO_OCPP_V201) {
        auto txSvc = context->getModel201().getTransactionService();
        auto txSvcEvse = txSvc ? txSvc->getEvse(evseId) : nullptr;
        if (!txSvcEvse) {
            MO_DBG_ERR("OOM");
            return false;
        }
        success = txSvcEvse->endAuthorization(MicroOcpp::v201::IdToken(idToken, type), /*validateIdToken*/ true, nullptr);
    }
    #endif

    return success;
}

bool mo_deauthorizeTransaction3(MO_Context *ctx, unsigned int evseId, const char *idToken, MO_IdTokenType type, bool validateIdToken, const char *groupIdToken) {
    if (!ctx) {
        MO_DBG_ERR("OCPP uninitialized"); //need to call mocpp_initialize before
        return false;
    }
    auto context = mo_getContext2(ctx);

    bool success = false;

    #if MO_ENABLE_V16
    if (context->getOcppVersion() == MO_OCPP_V16) {
        if (validateIdToken) {
            success = mo_endTransaction2(ctx, evseId, idToken, "Local");
        } else {
            success = mo_endTransaction_authorized2(ctx, evseId, idToken, "Local");
        }
    }
    #endif
    #if MO_ENABLE_V201
    if (context->getOcppVersion() == MO_OCPP_V201) {
        auto txSvc = context->getModel201().getTransactionService();
        auto txSvcEvse = txSvc ? txSvc->getEvse(evseId) : nullptr;
        if (!txSvcEvse) {
            MO_DBG_ERR("OOM");
            return false;
        }
        success = txSvcEvse->endAuthorization(MicroOcpp::v201::IdToken(idToken, type), validateIdToken, groupIdToken);
    }
    #endif

    return success;
}

//Force transaction stop
bool mo_abortTransaction(MO_TxStoppedReason stoppedReason, MO_TxEventTriggerReason stopTrigger) {
    return mo_abortTransaction2(mo_getApiContext(), EVSE_ID_1, stoppedReason, stopTrigger);
}

bool mo_abortTransaction2(MO_Context *ctx, unsigned int evseId, MO_TxStoppedReason stoppedReason, MO_TxEventTriggerReason stopTrigger) {
    if (!ctx) {
        MO_DBG_ERR("OCPP uninitialized"); //need to call mocpp_initialize before
        return false;
    }
    auto context = mo_getContext2(ctx);

    bool success = false;

    #if MO_ENABLE_V16
    if (context->getOcppVersion() == MO_OCPP_V16) {
        const char *reason = "";
        switch (stoppedReason) {
            case MO_TxStoppedReason_EmergencyStop:
                reason = "EmergencyStop";
                break;
            case MO_TxStoppedReason_PowerLoss:
                reason = "PowerLoss";
                break;
            case MO_TxStoppedReason_Reboot:
                reason = "Reboot";
                break;
            default:
                reason = "Other";
                break;
        }
        success = mo_endTransaction_authorized2(ctx, evseId, NULL, reason);
    }
    #endif
    #if MO_ENABLE_V201
    if (context->getOcppVersion() == MO_OCPP_V201) {
        auto txSvc = context->getModel201().getTransactionService();
        auto txSvcEvse = txSvc ? txSvc->getEvse(evseId) : nullptr;
        if (!txSvcEvse) {
            MO_DBG_ERR("OOM");
            return false;
        }
        success = txSvcEvse->abortTransaction(stoppedReason, stopTrigger);
    }
    #endif

    return success;
}
#endif //MO_ENABLE_V201

//Returns if according to OCPP, the EVSE is allowed to close provide charge now
bool mo_ocppPermitsCharge() {
    return mo_ocppPermitsCharge2(mo_getApiContext(), EVSE_ID_1);
}

bool mo_ocppPermitsCharge2(MO_Context *ctx, unsigned int evseId) {
    if (!ctx) {
        MO_DBG_ERR("OCPP uninitialized"); //need to call mocpp_initialize before
        return false;
    }
    auto context = mo_getContext2(ctx);

    bool res = false;

    #if MO_ENABLE_V16
    if (context->getOcppVersion() == MO_OCPP_V16) {
        auto txSvc = context->getModel16().getTransactionService();
        auto txSvcEvse = txSvc ? txSvc->getEvse(evseId) : nullptr;
        if (!txSvcEvse) {
            MO_DBG_ERR("init failure");
            return false;
        }
        res = txSvcEvse->ocppPermitsCharge();
    }
    #endif
    #if MO_ENABLE_V201
    if (context->getOcppVersion() == MO_OCPP_V201) {
        auto txSvc = context->getModel201().getTransactionService();
        auto txSvcEvse = txSvc ? txSvc->getEvse(evseId) : nullptr;
        if (!txSvcEvse) {
            MO_DBG_ERR("init failure");
            return false;
        }
        res = txSvcEvse->ocppPermitsCharge();
    }
    #endif

    return res;
}

//Get information about the current Transaction lifecycle
bool mo_isTransactionActive() {
    return mo_isTransactionActive2(mo_getApiContext(), EVSE_ID_1);
}

bool mo_isTransactionActive2(MO_Context *ctx, unsigned int evseId) {
    if (!ctx) {
        MO_DBG_ERR("OCPP uninitialized"); //need to call mocpp_initialize before
        return false;
    }
    auto context = mo_getContext2(ctx);

    bool res = false;

    #if MO_ENABLE_V16
    if (context->getOcppVersion() == MO_OCPP_V16) {
        auto txSvc = context->getModel16().getTransactionService();
        auto txSvcEvse = txSvc ? txSvc->getEvse(evseId) : nullptr;
        if (!txSvcEvse) {
            MO_DBG_ERR("init failure");
            return false;
        }
        auto tx = txSvcEvse->getTransaction();
        res = (tx && tx->isActive());
    }
    #endif
    #if MO_ENABLE_V201
    if (context->getOcppVersion() == MO_OCPP_V201) {
        auto txSvc = context->getModel201().getTransactionService();
        auto txSvcEvse = txSvc ? txSvc->getEvse(evseId) : nullptr;
        if (!txSvcEvse) {
            MO_DBG_ERR("init failure");
            return false;
        }
        auto tx = txSvcEvse->getTransaction();
        res = (tx && tx->active);
    }
    #endif

    return res;
}

bool mo_isTransactionRunning() {
    return mo_isTransactionRunning2(mo_getApiContext(), EVSE_ID_1);
}

bool mo_isTransactionRunning2(MO_Context *ctx, unsigned int evseId) {
    if (!ctx) {
        MO_DBG_ERR("OCPP uninitialized"); //need to call mocpp_initialize before
        return false;
    }
    auto context = mo_getContext2(ctx);

    bool res = false;

    #if MO_ENABLE_V16
    if (context->getOcppVersion() == MO_OCPP_V16) {
        auto txSvc = context->getModel16().getTransactionService();
        auto txSvcEvse = txSvc ? txSvc->getEvse(evseId) : nullptr;
        if (!txSvcEvse) {
            MO_DBG_ERR("init failure");
            return false;
        }
        auto tx = txSvcEvse->getTransaction();
        res = (tx && tx->isRunning());
    }
    #endif
    #if MO_ENABLE_V201
    if (context->getOcppVersion() == MO_OCPP_V201) {
        auto txSvc = context->getModel201().getTransactionService();
        auto txSvcEvse = txSvc ? txSvc->getEvse(evseId) : nullptr;
        if (!txSvcEvse) {
            MO_DBG_ERR("init failure");
            return false;
        }
        auto tx = txSvcEvse->getTransaction();
        res = (tx && tx->started && !tx->stopped);
    }
    #endif

    return res;
}

/*
 * Get the idTag which has been used to start the transaction. If no transaction process is
 * running, this function returns nullptr
 */
const char *mo_getTransactionIdTag() {
    return mo_getTransactionIdTag2(mo_getApiContext(), EVSE_ID_1);
}

const char *mo_getTransactionIdTag2(MO_Context *ctx, unsigned int evseId) {
    if (!ctx) {
        MO_DBG_ERR("OCPP uninitialized"); //need to call mocpp_initialize before
        return nullptr;
    }
    auto context = mo_getContext2(ctx);

    const char *res = nullptr;

    #if MO_ENABLE_V16
    if (context->getOcppVersion() == MO_OCPP_V16) {
        auto txSvc = context->getModel16().getTransactionService();
        auto txSvcEvse = txSvc ? txSvc->getEvse(evseId) : nullptr;
        if (!txSvcEvse) {
            MO_DBG_ERR("init failure");
            return nullptr;
        }
        if (auto tx = txSvcEvse->getTransaction()) {
            res = tx->getIdTag();
        }
    }
    #endif
    #if MO_ENABLE_V201
    if (context->getOcppVersion() == MO_OCPP_V201) {
        auto txSvc = context->getModel201().getTransactionService();
        auto txSvcEvse = txSvc ? txSvc->getEvse(evseId) : nullptr;
        if (!txSvcEvse) {
            MO_DBG_ERR("init failure");
            return nullptr;
        }
        if (auto tx = txSvcEvse->getTransaction()) {
            res = tx->idToken.get();
        }
    }
    #endif

    return res && *res ? res : nullptr;
}

#if MO_ENABLE_V201
const char *mo_getTransactionId() {
    return mo_getTransactionId2(mo_getApiContext(), EVSE_ID_1);
}

const char *mo_getTransactionId2(MO_Context *ctx, unsigned int evseId) {
    if (!ctx) {
        MO_DBG_ERR("OCPP uninitialized"); //need to call mocpp_initialize before
        return nullptr;
    }
    auto context = mo_getContext2(ctx);

    const char *res = nullptr;

    #if MO_ENABLE_V16
    if (context->getOcppVersion() == MO_OCPP_V16) {
        auto txSvc = context->getModel16().getTransactionService();
        auto txSvcEvse = txSvc ? txSvc->getEvse(evseId) : nullptr;
        if (!txSvcEvse) {
            MO_DBG_ERR("init failure");
            return nullptr;
        }
        if (auto tx = txSvcEvse->getTransaction()) {
            res = tx->getTransactionIdCompat();
        }
    }
    #endif
    #if MO_ENABLE_V201
    if (context->getOcppVersion() == MO_OCPP_V201) {
        auto txSvc = context->getModel201().getTransactionService();
        auto txSvcEvse = txSvc ? txSvc->getEvse(evseId) : nullptr;
        if (!txSvcEvse) {
            MO_DBG_ERR("init failure");
            return nullptr;
        }
        if (auto tx = txSvcEvse->getTransaction()) {
            res = tx->transactionId;
        }
    }
    #endif

    return res && *res ? res : nullptr;
}
#endif //MO_ENABLE_V201

#if MO_ENABLE_V16
int mo_v16_getTransactionId() {
    return mo_v16_getTransactionId2(mo_getApiContext(), EVSE_ID_1);
}

int mo_v16_getTransactionId2(MO_Context *ctx, unsigned int evseId) {
    if (!ctx) {
        MO_DBG_ERR("OCPP uninitialized"); //need to call mocpp_initialize before
        return -1;
    }
    auto context = mo_getContext2(ctx);

    int res = -1;

    #if MO_ENABLE_V16
    if (context->getOcppVersion() == MO_OCPP_V16) {
        auto txSvc = context->getModel16().getTransactionService();
        auto txSvcEvse = txSvc ? txSvc->getEvse(evseId) : nullptr;
        if (!txSvcEvse) {
            MO_DBG_ERR("init failure");
            return -1;
        }
        if (auto tx = txSvcEvse->getTransaction()) {
            res = tx->getTransactionId();
        }
    }
    #endif
    #if MO_ENABLE_V201
    if (context->getOcppVersion() == MO_OCPP_V201) {
        MO_DBG_ERR("mo_v16_getTransactionId not supported with OCPP 2.0.1");
    }
    #endif

    return res;
}
#endif //MO_ENABLE_V16

MO_ChargePointStatus mo_getChargePointStatus() {
    return mo_getChargePointStatus2(mo_getApiContext(), EVSE_ID_1);
}

MO_ChargePointStatus mo_getChargePointStatus2(MO_Context *ctx, unsigned int evseId) {
    if (!ctx) {
        MO_DBG_ERR("OCPP uninitialized"); //need to call mocpp_initialize before
        return MO_ChargePointStatus_UNDEFINED;
    }
    auto context = mo_getContext2(ctx);

    MO_ChargePointStatus res = MO_ChargePointStatus_UNDEFINED;

    #if MO_ENABLE_V16
    if (context->getOcppVersion() == MO_OCPP_V16) {
        auto availSvc = context->getModel16().getAvailabilityService();
        auto availSvcEvse = availSvc ? availSvc->getEvse(evseId) : nullptr;
        if (!availSvcEvse) {
            MO_DBG_ERR("init failure");
            return MO_ChargePointStatus_UNDEFINED;
        }
        res = availSvcEvse->getStatus();
    }
    #endif
    #if MO_ENABLE_V201
    if (context->getOcppVersion() == MO_OCPP_V201) {
        auto availSvc = context->getModel201().getAvailabilityService();
        auto availSvcEvse = availSvc ? availSvc->getEvse(evseId) : nullptr;
        if (!availSvcEvse) {
            MO_DBG_ERR("init failure");
            return MO_ChargePointStatus_UNDEFINED;
        }
        res = availSvcEvse->getStatus();
    }
    #endif

    return res;
}

//Input if EV is ready to charge (= J1772 State C)
void mo_setEvReadyInput(bool (*evReadyInput)()) {
    //convert and forward callback to `mo_setEvReadyInput2()`
    mo_setEvReadyInput2(mo_getApiContext(), EVSE_ID_1, [] (unsigned int, void *userData) {
        auto evReadyInput = reinterpret_cast<bool (*)()>(userData);
        return evReadyInput();
    }, reinterpret_cast<void*>(evReadyInput));
}

void mo_setEvReadyInput2(MO_Context *ctx, unsigned int evseId, bool (*evReadyInput2)(unsigned int, void*), void *userData) {
    if (!ctx) {
        MO_DBG_ERR("OCPP uninitialized"); //need to call mocpp_initialize before
        return;
    }
    auto context = mo_getContext2(ctx);

    #if MO_ENABLE_V16
    if (context->getOcppVersion() == MO_OCPP_V16) {
        auto availSvc = context->getModel16().getAvailabilityService();
        auto availSvcEvse = availSvc ? availSvc->getEvse(evseId) : nullptr;
        if (!availSvcEvse) {
            MO_DBG_ERR("init failure");
            return;
        }
        availSvcEvse->setEvReadyInput(evReadyInput2, userData);

        auto txSvc = context->getModel16().getTransactionService();
        auto txSvcEvse = txSvc ? txSvc->getEvse(evseId) : nullptr;
        if (!txSvcEvse) {
            MO_DBG_ERR("init failure");
            return;
        }
        txSvcEvse->setEvReadyInput(evReadyInput2, userData);
    }
    #endif
    #if MO_ENABLE_V201
    if (context->getOcppVersion() == MO_OCPP_V201) {
        auto txSvc = context->getModel201().getTransactionService();
        auto txSvcEvse = txSvc ? txSvc->getEvse(evseId) : nullptr;
        if (!txSvcEvse) {
            MO_DBG_ERR("init failure");
            return;
        }
        txSvcEvse->setEvReadyInput(evReadyInput2, userData);
    }
    #endif
}

//Input if EVSE allows charge (= PWM signal on)
void mo_setEvseReadyInput(bool (*evseReadyInput)()) {
    //convert and forward callback to `mo_setEvseReadyInput2()`
    mo_setEvseReadyInput2(mo_getApiContext(), EVSE_ID_1, [] (unsigned int, void *userData) {
        auto evseReadyInput = reinterpret_cast<bool (*)()>(userData);
        return evseReadyInput();
    }, reinterpret_cast<void*>(evseReadyInput));
}

void mo_setEvseReadyInput2(MO_Context *ctx, unsigned int evseId, bool (*evseReadyInput2)(unsigned int, void*), void *userData) {
    if (!ctx) {
        MO_DBG_ERR("OCPP uninitialized"); //need to call mocpp_initialize before
        return;
    }
    auto context = mo_getContext2(ctx);

    #if MO_ENABLE_V16
    if (context->getOcppVersion() == MO_OCPP_V16) {
        auto availSvc = context->getModel16().getAvailabilityService();
        auto availSvcEvse = availSvc ? availSvc->getEvse(evseId) : nullptr;
        if (!availSvcEvse) {
            MO_DBG_ERR("init failure");
            return;
        }
        availSvcEvse->setEvseReadyInput(evseReadyInput2, userData);
    }
    #endif
    #if MO_ENABLE_V201
    if (context->getOcppVersion() == MO_OCPP_V201) {
        auto txSvc = context->getModel201().getTransactionService();
        auto txSvcEvse = txSvc ? txSvc->getEvse(evseId) : nullptr;
        if (!txSvcEvse) {
            MO_DBG_ERR("init failure");
            return;
        }
        txSvcEvse->setEvseReadyInput(evseReadyInput2, userData);
    }
    #endif
}

//Input for Error codes (please refer to OCPP 1.6, Edit2, p. 71 and 72 for valid error codes)
bool mo_addErrorCodeInput(const char* (*errorCodeInput)()) {
    if (!g_context) {
        MO_DBG_ERR("OCPP uninitialized"); //need to call mocpp_initialize before
        return false;
    }
    auto context = g_context;

    bool success = false;

    #if MO_ENABLE_V16
    if (context->getOcppVersion() == MO_OCPP_V16) {
        auto availSvc = context->getModel16().getAvailabilityService();
        auto availSvcEvse = availSvc ? availSvc->getEvse(EVSE_ID_1) : nullptr;
        if (!availSvcEvse) {
            MO_DBG_ERR("init failure");
            return false;
        }

        MO_ErrorDataInput errorDataInput;
        mo_errorDataInput_init(&errorDataInput);
        errorDataInput.userData = reinterpret_cast<void*>(errorCodeInput);
        errorDataInput.getErrorData = [] (unsigned int, void *userData) {
            auto errorCodeInput = reinterpret_cast<const char* (*)()>(userData);

            MO_ErrorData errorData;
            mo_errorData_init(&errorData);
            mo_errorData_setErrorCode(&errorData, errorCodeInput());
            return errorData;
        };

        success = availSvcEvse->addErrorDataInput(errorDataInput);
    }
    #endif
    #if MO_ENABLE_V201
    if (context->getOcppVersion() == MO_OCPP_V201) {
        auto availSvc = context->getModel201().getAvailabilityService();
        auto availSvcEvse = availSvc ? availSvc->getEvse(EVSE_ID_1) : nullptr;
        if (!availSvcEvse) {
            MO_DBG_ERR("init failure");
            return false;
        }

        //error codes not really supported in OCPP 2.0.1, just Faulted status
        MicroOcpp::v201::FaultedInput faultedInput;
        faultedInput.userData = reinterpret_cast<void*>(errorCodeInput);
        faultedInput.isFaulted = [] (unsigned int evseId, void *userData) {
            auto errorCodeInput = reinterpret_cast<const char* (*)()>(userData);

            return errorCodeInput() != nullptr;
        };

        success = availSvcEvse->addFaultedInput(faultedInput);
    }
    #endif

    return success;
}

#if MO_ENABLE_V16
//Input for Error codes + additional error data. See "Availability.h" for more docs
bool mo_v16_addErrorDataInput(MO_Context *ctx, unsigned int evseId, MO_ErrorData (*errorData)(unsigned int, void*), void *userData) {
    if (!ctx) {
        MO_DBG_ERR("OCPP uninitialized"); //need to call mocpp_initialize before
        return false;
    }
    auto context = mo_getContext2(ctx);

    bool success = false;

    #if MO_ENABLE_V16
    if (context->getOcppVersion() == MO_OCPP_V16) {
        auto availSvc = context->getModel16().getAvailabilityService();
        auto availSvcEvse = availSvc ? availSvc->getEvse(evseId) : nullptr;
        if (!availSvcEvse) {
            MO_DBG_ERR("init failure");
            return false;
        }

        MO_ErrorDataInput errorDataInput;
        mo_errorDataInput_init(&errorDataInput);
        errorDataInput.getErrorData = errorData;
        errorDataInput.userData = userData;

        success = availSvcEvse->addErrorDataInput(errorDataInput);
    }
    #endif
    #if MO_ENABLE_V201
    if (context->getOcppVersion() == MO_OCPP_V201) {
        MO_DBG_ERR("mo_v16_addErrorDataInput not supported with OCPP 2.0.1");
    }
    #endif

    return success;
}
#endif //MO_ENABLE_V16

#if MO_ENABLE_V201
//Input to set EVSE into Faulted state
bool mo_v201_addFaultedInput(MO_Context *ctx, unsigned int evseId, bool (*faulted)(unsigned int, void*), void *userData) {
    if (!ctx) {
        MO_DBG_ERR("OCPP uninitialized"); //need to call mocpp_initialize before
        return false;
    }
    auto context = mo_getContext2(ctx);

    bool success = false;

    #if MO_ENABLE_V16
    if (context->getOcppVersion() == MO_OCPP_V16) {
        MO_DBG_ERR("mo_v201_addFaultedInput not supported with OCPP 1.6");
    }
    #endif
    #if MO_ENABLE_V201
    if (context->getOcppVersion() == MO_OCPP_V201) {
        auto availSvc = context->getModel201().getAvailabilityService();
        auto availSvcEvse = availSvc ? availSvc->getEvse(evseId) : nullptr;
        if (!availSvcEvse) {
            MO_DBG_ERR("init failure");
            return false;
        }

        //error codes not really supported in OCPP 2.0.1, just Faulted status
        MicroOcpp::v201::FaultedInput faultedInput;
        faultedInput.isFaulted = faulted;
        faultedInput.userData = userData;

        success = availSvcEvse->addFaultedInput(faultedInput);
    }
    #endif

    return success;
}
#endif //MO_ENABLE_V201

bool mo_addMeterValueInputInt(int32_t (*meterInput)(), const char *measurand, const char *unit, const char *location, const char *phase) {
    MO_MeterInput mInput;
    mo_meterInput_init(&mInput, MO_MeterInputType_Int);
    mInput.getInt = meterInput;
    mInput.measurand = measurand;
    mInput.unit = unit;
    mInput.location = location;
    mInput.phase = phase;
    return mo_addMeterValueInput(mo_getApiContext(), EVSE_ID_1, mInput);
}

bool mo_addMeterValueInputInt2(MO_Context *ctx, unsigned int evseId, int32_t (*meterInput)(MO_ReadingContext, unsigned int, void*), const char *measurand, const char *unit, const char *location, const char *phase, void *userData) {
    MO_MeterInput mInput;
    mo_meterInput_init(&mInput, MO_MeterInputType_IntWithArgs);
    mInput.getInt2 = meterInput;
    mInput.measurand = measurand;
    mInput.unit = unit;
    mInput.location = location;
    mInput.phase = phase;
    mInput.user_data = userData;
    return mo_addMeterValueInput(ctx, evseId, mInput);
}

bool mo_addMeterValueInputFloat(float (*meterInput)(), const char *measurand, const char *unit, const char *location, const char *phase) {
    MO_MeterInput mInput;
    mo_meterInput_init(&mInput, MO_MeterInputType_Float);
    mInput.getFloat = meterInput;
    mInput.measurand = measurand;
    mInput.unit = unit;
    mInput.location = location;
    mInput.phase = phase;
    return mo_addMeterValueInput(mo_getApiContext(), EVSE_ID_1, mInput);
}

bool mo_addMeterValueInputFloat2(MO_Context *ctx, unsigned int evseId, float (*meterInput)(MO_ReadingContext, unsigned int, void*), const char *measurand, const char *unit, const char *location, const char *phase, void *userData) {
    MO_MeterInput mInput;
    mo_meterInput_init(&mInput, MO_MeterInputType_FloatWithArgs);
    mInput.getFloat2 = meterInput;
    mInput.measurand = measurand;
    mInput.unit = unit;
    mInput.location = location;
    mInput.phase = phase;
    mInput.user_data = userData;
    return mo_addMeterValueInput(ctx, evseId, mInput);
}

bool mo_addMeterValueInputString(int (*meterInput)(char *buf, size_t size), const char *measurand, const char *unit, const char *location, const char *phase) {
    MO_MeterInput mInput;
    mo_meterInput_init(&mInput, MO_MeterInputType_String);
    mInput.getString = meterInput;
    mInput.measurand = measurand;
    mInput.unit = unit;
    mInput.location = location;
    mInput.phase = phase;
    return mo_addMeterValueInput(mo_getApiContext(), EVSE_ID_1, mInput);
}
bool mo_addMeterValueInputString2(MO_Context *ctx, unsigned int evseId, int (*meterInput)(char *buf, size_t size, MO_ReadingContext, unsigned int, void*), const char *measurand, const char *unit, const char *location, const char *phase, void *userData) {
    MO_MeterInput mInput;
    mo_meterInput_init(&mInput, MO_MeterInputType_StringWithArgs);
    mInput.getString2 = meterInput;
    mInput.measurand = measurand;
    mInput.unit = unit;
    mInput.location = location;
    mInput.phase = phase;
    mInput.user_data = userData;
    return mo_addMeterValueInput(ctx, evseId, mInput);
}

//Input for further MeterValue types with more options. See "MeterValue.h" for how to use it
bool mo_addMeterValueInput(MO_Context *ctx, unsigned int evseId, MO_MeterInput mInput) {
    if (!ctx) {
        MO_DBG_ERR("OCPP uninitialized"); //need to call mocpp_initialize before
        return false;
    }
    auto context = mo_getContext2(ctx);

    bool success = false;

    #if MO_ENABLE_V16
    if (context->getOcppVersion() == MO_OCPP_V16) {
        auto meterSvc = context->getModel16().getMeteringService();
        auto meterSvcEvse = meterSvc ? meterSvc->getEvse(evseId) : nullptr;
        if (!meterSvcEvse) {
            MO_DBG_ERR("init failure");
            return false;
        }
        success = meterSvcEvse->addMeterInput(mInput);
    }
    #endif
    #if MO_ENABLE_V201
    if (context->getOcppVersion() == MO_OCPP_V201) {
        auto meterSvc = context->getModel201().getMeteringService();
        auto meterSvcEvse = meterSvc ? meterSvc->getEvse(evseId) : nullptr;
        if (!meterSvcEvse) {
            MO_DBG_ERR("init failure");
            return false;
        }
        success = meterSvcEvse->addMeterInput(mInput);
    }
    #endif

    return success;
}

void mo_setOccupiedInput(bool (*occupied)()) {
    //convert and forward callback to `mo_setOccupiedInput2()`
    mo_setOccupiedInput2(mo_getApiContext(), EVSE_ID_1, [] (unsigned int, void *userData) {
        auto occupied = reinterpret_cast<bool (*)()>(userData);
        return occupied();
    }, reinterpret_cast<void*>(occupied));
}

void mo_setOccupiedInput2(MO_Context *ctx, unsigned int evseId, bool (*occupied2)(unsigned int, void*), void *userData) {
    if (!ctx) {
        MO_DBG_ERR("OCPP uninitialized"); //need to call mocpp_initialize before
        return;
    }
    auto context = mo_getContext2(ctx);

    #if MO_ENABLE_V16
    if (context->getOcppVersion() == MO_OCPP_V16) {
        auto availSvc = context->getModel16().getAvailabilityService();
        auto availSvcEvse = availSvc ? availSvc->getEvse(evseId) : nullptr;
        if (!availSvcEvse) {
            MO_DBG_ERR("init failure");
            return;
        }
        availSvcEvse->setOccupiedInput(occupied2, userData);
    }
    #endif
    #if MO_ENABLE_V201
    if (context->getOcppVersion() == MO_OCPP_V201) {
        auto availSvc = context->getModel201().getAvailabilityService();
        auto availSvcEvse = availSvc ? availSvc->getEvse(evseId) : nullptr;
        if (!availSvcEvse) {
            MO_DBG_ERR("init failure");
            return;
        }
        availSvcEvse->setOccupiedInput(occupied2, userData);
    }
    #endif
}

//Input if the charger is ready for StartTransaction
void mo_setStartTxReadyInput(bool (*startTxReady)()) {
    //convert and forward callback to `mo_setStartTxReadyInput2()`
    mo_setStartTxReadyInput2(mo_getApiContext(), EVSE_ID_1, [] (unsigned int, void *userData) {
        auto startTxReady = reinterpret_cast<bool (*)()>(userData);
        return startTxReady();
    }, reinterpret_cast<void*>(startTxReady));
}

void mo_setStartTxReadyInput2(MO_Context *ctx, unsigned int evseId, bool (*startTxReady2)(unsigned int, void*), void *userData) {
    if (!ctx) {
        MO_DBG_ERR("OCPP uninitialized"); //need to call mocpp_initialize before
        return;
    }
    auto context = mo_getContext2(ctx);

    #if MO_ENABLE_V16
    if (context->getOcppVersion() == MO_OCPP_V16) {
        auto txSvc = context->getModel16().getTransactionService();
        auto txSvcEvse = txSvc ? txSvc->getEvse(evseId) : nullptr;
        if (!txSvcEvse) {
            MO_DBG_ERR("init failure");
            return;
        }
        txSvcEvse->setStartTxReadyInput(startTxReady2, userData);
    }
    #endif
    #if MO_ENABLE_V201
    if (context->getOcppVersion() == MO_OCPP_V201) {
        auto txSvc = context->getModel201().getTransactionService();
        auto txSvcEvse = txSvc ? txSvc->getEvse(evseId) : nullptr;
        if (!txSvcEvse) {
            MO_DBG_ERR("init failure");
            return;
        }
        txSvcEvse->setStartTxReadyInput(startTxReady2, userData);
    }
    #endif
}

//Input if the charger is ready for StopTransaction
void mo_setStopTxReadyInput(bool (*stopTxReady)()) {
    //convert and forward callback to `mo_setStopTxReadyInput2()`
    mo_setStopTxReadyInput2(mo_getApiContext(), EVSE_ID_1, [] (unsigned int, void *userData) {
        auto stopTxReady = reinterpret_cast<bool (*)()>(userData);
        return stopTxReady();
    }, reinterpret_cast<void*>(stopTxReady));
}

void mo_setStopTxReadyInput2(MO_Context *ctx, unsigned int evseId, bool (*stopTxReady2)(unsigned int, void*), void *userData) {
    if (!ctx) {
        MO_DBG_ERR("OCPP uninitialized"); //need to call mocpp_initialize before
        return;
    }
    auto context = mo_getContext2(ctx);

    #if MO_ENABLE_V16
    if (context->getOcppVersion() == MO_OCPP_V16) {
        auto txSvc = context->getModel16().getTransactionService();
        auto txSvcEvse = txSvc ? txSvc->getEvse(evseId) : nullptr;
        if (!txSvcEvse) {
            MO_DBG_ERR("init failure");
            return;
        }
        txSvcEvse->setStopTxReadyInput(stopTxReady2, userData);
    }
    #endif
    #if MO_ENABLE_V201
    if (context->getOcppVersion() == MO_OCPP_V201) {
        auto txSvc = context->getModel201().getTransactionService();
        auto txSvcEvse = txSvc ? txSvc->getEvse(evseId) : nullptr;
        if (!txSvcEvse) {
            MO_DBG_ERR("init failure");
            return;
        }
        txSvcEvse->setStopTxReadyInput(stopTxReady2, userData);
    }
    #endif
}

//Called when transaction state changes (see "TransactionDefs.h" for possible events). Transaction can be null
void mo_setTxNotificationOutput(void (*txNotificationOutput)(MO_TxNotification)) {
    //convert and forward callback to `mo_setTxNotificationOutput2()`
    mo_setTxNotificationOutput2(mo_getApiContext(), EVSE_ID_1, [] (MO_TxNotification txNotification, unsigned int, void *userData) {
        auto txNotificationOutput = reinterpret_cast<bool (*)(MO_TxNotification)>(userData);
        txNotificationOutput(txNotification);
    }, reinterpret_cast<void*>(txNotificationOutput));
}

void mo_setTxNotificationOutput2(MO_Context *ctx, unsigned int evseId, void (*txNotificationOutput2)(MO_TxNotification, unsigned int, void*), void *userData) {
    if (!ctx) {
        MO_DBG_ERR("OCPP uninitialized"); //need to call mocpp_initialize before
        return;
    }
    auto context = mo_getContext2(ctx);

    #if MO_ENABLE_V16
    if (context->getOcppVersion() == MO_OCPP_V16) {
        auto txSvc = context->getModel16().getTransactionService();
        auto txSvcEvse = txSvc ? txSvc->getEvse(evseId) : nullptr;
        if (!txSvcEvse) {
            MO_DBG_ERR("init failure");
            return;
        }
        txSvcEvse->setTxNotificationOutput(txNotificationOutput2, userData);
    }
    #endif
    #if MO_ENABLE_V201
    if (context->getOcppVersion() == MO_OCPP_V201) {
        auto txSvc = context->getModel201().getTransactionService();
        auto txSvcEvse = txSvc ? txSvc->getEvse(evseId) : nullptr;
        if (!txSvcEvse) {
            MO_DBG_ERR("init failure");
            return;
        }
        txSvcEvse->setTxNotificationOutput(txNotificationOutput2, userData);
    }
    #endif
}

#if MO_ENABLE_CONNECTOR_LOCK
void mo_setOnUnlockConnector(MO_UnlockConnectorResult (*onUnlockConnector)()) {
    //convert and forward callback to `mo_setOnUnlockConnector2()`
    mo_setOnUnlockConnector2(mo_getApiContext(), EVSE_ID_1, [] (unsigned int, void *userData) {
        auto onUnlockConnector = reinterpret_cast<MO_UnlockConnectorResult (*)()>(userData);
        return onUnlockConnector();
    }, reinterpret_cast<void*>(onUnlockConnector));
}

void mo_setOnUnlockConnector2(MO_Context *ctx, unsigned int evseId, MO_UnlockConnectorResult (*onUnlockConnector2)(unsigned int, void*), void *userData) {
    if (!ctx) {
        MO_DBG_ERR("OCPP uninitialized"); //need to call mocpp_initialize before
        return;
    }
    auto context = mo_getContext2(ctx);

    auto rmtSvc = context->getModelCommon().getRemoteControlService();
    auto rmtSvcEvse = rmtSvc ? rmtSvc->getEvse(evseId) : nullptr;
    if (!rmtSvcEvse) {
        MO_DBG_ERR("init failure");
        return;
    }
    rmtSvcEvse->setOnUnlockConnector(onUnlockConnector2, userData);
}
#endif //MO_ENABLE_CONNECTOR_LOCK

bool mo_isOperative() {
    return mo_isOperative2(mo_getApiContext(), EVSE_ID_1);
}

bool mo_isOperative2(MO_Context *ctx, unsigned int evseId) {
    if (!ctx) {
        MO_DBG_ERR("OCPP uninitialized"); //need to call mocpp_initialize before
        return false;
    }
    auto context = mo_getContext2(ctx);

    bool result = false;

    #if MO_ENABLE_V16
    if (context->getOcppVersion() == MO_OCPP_V16) {
        auto availSvc = context->getModel16().getAvailabilityService();
        auto availSvcEvse = availSvc ? availSvc->getEvse(evseId) : nullptr;
        if (!availSvcEvse) {
            MO_DBG_ERR("init failure");
            return false;
        }
        result = availSvcEvse->isOperative();
    }
    #endif
    #if MO_ENABLE_V201
    if (context->getOcppVersion() == MO_OCPP_V201) {
        auto availSvc = context->getModel201().getAvailabilityService();
        auto availSvcEvse = availSvc ? availSvc->getEvse(evseId) : nullptr;
        if (!availSvcEvse) {
            MO_DBG_ERR("init failure");
            return false;
        }
        result = availSvcEvse->isOperative();
    }
    #endif

    return result;
}

bool mo_isConnected() {
    return mo_isConnected2(mo_getApiContext());
}
bool mo_isConnected2(MO_Context *ctx) {
    if (!ctx) {
        MO_DBG_ERR("OCPP uninitialized"); //need to call mocpp_initialize before
        return false;
    }
    auto context = mo_getContext2(ctx);

    if (auto connection = context->getConnection()) {
        return connection->isConnected();
    } else {
        MO_DBG_ERR("WebSocket not set up");
        return false;
    }
}

bool mo_isAcceptedByCsms() {
    return mo_isAcceptedByCsms2(mo_getApiContext());
}

bool mo_isAcceptedByCsms2(MO_Context *ctx) {
    if (!ctx) {
        MO_DBG_ERR("OCPP uninitialized"); //need to call mocpp_initialize before
        return false;
    }
    auto context = mo_getContext2(ctx);

    MicroOcpp::BootService *bootService = context->getModelCommon().getBootService();
    if (!bootService) {
        MO_DBG_ERR("OOM");
        return false;
    }

    return (bootService->getRegistrationStatus() == MicroOcpp::RegistrationStatus::Accepted);
}

int32_t mo_getUnixTime() {
    return mo_getUnixTime2(mo_getApiContext());
}

int32_t mo_getUnixTime2(MO_Context *ctx) {
    if (!ctx) {
        MO_DBG_ERR("OCPP uninitialized"); //need to call mocpp_initialize before
        return 0;
    }
    auto context = mo_getContext2(ctx);
    auto& clock = context->getClock();

    int32_t unixTime = 0;
    if (!clock.toUnixTime(clock.now(), unixTime)) {
        unixTime = 0;
    }

    return unixTime;
}

bool mo_setUnixTime(int32_t unixTime) {
    return mo_setUnixTime2(mo_getApiContext(), unixTime);
}

bool mo_setUnixTime2(MO_Context *ctx, int32_t unixTime) {
    if (!ctx) {
        MO_DBG_ERR("OCPP uninitialized"); //need to call mocpp_initialize before
        return false;
    }
    auto context = mo_getContext2(ctx);

    return context->getClock().setTime(unixTime);
}

int32_t mo_getUptime() {
    return mo_getUptime2(mo_getApiContext());
}

int32_t mo_getUptime2(MO_Context *ctx) {
    if (!ctx) {
        MO_DBG_ERR("OCPP uninitialized"); //need to call mocpp_initialize before
        return 0;
    }
    auto context = mo_getContext2(ctx);

    return context->getClock().getUptimeInt();
}

int mo_getOcppVersion() {
    return mo_getOcppVersion2(mo_getApiContext());
}

int mo_getOcppVersion2(MO_Context *ctx) {
    if (!ctx) {
        MO_DBG_ERR("OCPP uninitialized"); //need to call mocpp_initialize before
        return -1;
    }
    auto context = mo_getContext2(ctx);

    return context->getOcppVersion();
}

void mo_setOnResetExecute(void (*onResetExecute)()) {
    if (!g_context) {
        MO_DBG_ERR("OCPP uninitialized"); //need to call mocpp_initialize before
        return;
    }
    auto context = g_context;

    #if MO_ENABLE_V16
    if (context->getOcppVersion() == MO_OCPP_V16) {
        auto rstSvc = context->getModel16().getResetService();
        if (!rstSvc) {
            MO_DBG_ERR("init failure");
            return;
        }
        rstSvc->setExecuteReset([] (bool, void *userData) {
            auto onResetExecute = reinterpret_cast<void(*)()>(userData);
            onResetExecute();
            return true;
        }, reinterpret_cast<void*>(onResetExecute));
    }
    #endif
    #if MO_ENABLE_V201
    if (context->getOcppVersion() == MO_OCPP_V201) {
        auto rstSvc = context->getModel201().getResetService();
        if (!rstSvc) {
            MO_DBG_ERR("init failure");
            return;
        }
        rstSvc->setExecuteReset(EVSE_ID_0, [] (unsigned int, void *userData) {
            auto onResetExecute = reinterpret_cast<void(*)()>(userData);
            onResetExecute();
            return true;
        }, reinterpret_cast<void*>(onResetExecute));
    }
    #endif
}

#if MO_ENABLE_V16
void mo_v16_setOnResetNotify(bool (*onResetNotify)(bool)) {
    mo_v16_setOnResetNotify2(mo_getApiContext(), [] (bool isHard, void *userData) {
        auto onResetNotify = reinterpret_cast<bool (*)(bool)>(userData);
        return onResetNotify(isHard);
    }, reinterpret_cast<void*>(onResetNotify));
}

void mo_v16_setOnResetNotify2(MO_Context *ctx, bool (*onResetNotify2)(bool, void*), void *userData) {
    if (!ctx) {
        MO_DBG_ERR("OCPP uninitialized"); //need to call mocpp_initialize before
        return;
    }
    auto context = mo_getContext2(ctx);

    #if MO_ENABLE_V16
    if (context->getOcppVersion() == MO_OCPP_V16) {
        auto rstSvc = context->getModel16().getResetService();
        if (!rstSvc) {
            MO_DBG_ERR("init failure");
            return;
        }
        rstSvc->setNotifyReset(onResetNotify2, userData);
    }
    #endif
    #if MO_ENABLE_V201
    if (context->getOcppVersion() == MO_OCPP_V201) {
        MO_DBG_ERR("mo_v16_setOnResetNotify not supported with OCPP 2.0.1");
    }
    #endif
}

void mo_v16_setOnResetExecute(void (*onResetExecute)(bool)) {
    mo_v16_setOnResetExecute2(mo_getApiContext(), [] (bool isHard, void *userData) {
        auto onResetExecute = reinterpret_cast<void (*)(bool)>(userData);
        onResetExecute(isHard);
        return true;
    }, reinterpret_cast<void*>(onResetExecute));
}
void mo_v16_setOnResetExecute2(MO_Context *ctx, bool (*onResetExecute2)(bool, void*), void *userData) {
    if (!ctx) {
        MO_DBG_ERR("OCPP uninitialized"); //need to call mocpp_initialize before
        return;
    }
    auto context = mo_getContext2(ctx);

    #if MO_ENABLE_V16
    if (context->getOcppVersion() == MO_OCPP_V16) {
        auto rstSvc = context->getModel16().getResetService();
        if (!rstSvc) {
            MO_DBG_ERR("init failure");
            return;
        }
        rstSvc->setExecuteReset(onResetExecute2, userData);
    }
    #endif
    #if MO_ENABLE_V201
    if (context->getOcppVersion() == MO_OCPP_V201) {
        MO_DBG_ERR("mo_v16_setOnResetExecute not supported with OCPP 2.0.1");
    }
    #endif
}
#endif //MO_ENABLE_V16

#if MO_ENABLE_V201
void mo_v201_setOnResetNotify(bool (*onResetNotify)(MO_ResetType)) {
    //convert and forward callback to `mo_v201_setOnResetNotify2()`
    mo_v201_setOnResetNotify2(mo_getApiContext(), EVSE_ID_0, [] (MO_ResetType resetType, unsigned int, void *userData) {
        auto onResetNotify = reinterpret_cast<bool (*)(MO_ResetType)>(userData);
        return onResetNotify(resetType);
    }, reinterpret_cast<void*>(onResetNotify));
}

void mo_v201_setOnResetNotify2(MO_Context *ctx, unsigned int evseId, bool (*onResetNotify2)(MO_ResetType, unsigned int, void*), void *userData) {
    if (!ctx) {
        MO_DBG_ERR("OCPP uninitialized"); //need to call mocpp_initialize before
        return;
    }
    auto context = mo_getContext2(ctx);

    #if MO_ENABLE_V16
    if (context->getOcppVersion() == MO_OCPP_V16) {
        MO_DBG_ERR("mo_v201_setOnResetNotify not supported with OCPP 1.6");
    }
    #endif
    #if MO_ENABLE_V201
    if (context->getOcppVersion() == MO_OCPP_V201) {
        auto rstSvc = context->getModel201().getResetService();
        if (!rstSvc) {
            MO_DBG_ERR("init failure");
            return;
        }
        rstSvc->setNotifyReset(evseId, onResetNotify2, userData);
    }
    #endif
}

void mo_v201_setOnResetExecute(void (*onResetExecute)()) {
    //convert and forward callback to `mo_v201_setOnResetExecute2()`
    mo_v201_setOnResetExecute2(mo_getApiContext(), EVSE_ID_0, [] (unsigned int, void *userData) {
        auto onResetExecute = reinterpret_cast<void (*)()>(userData);
        onResetExecute();
        return false;
    }, reinterpret_cast<void*>(onResetExecute));
}

void mo_v201_setOnResetExecute2(MO_Context *ctx, unsigned int evseId, bool (*onResetExecute2)(unsigned int, void*), void *userData) {
    if (!ctx) {
        MO_DBG_ERR("OCPP uninitialized"); //need to call mocpp_initialize before
        return;
    }
    auto context = mo_getContext2(ctx);

    #if MO_ENABLE_V16
    if (context->getOcppVersion() == MO_OCPP_V16) {
        MO_DBG_ERR("mo_v201_setOnResetExecute not supported with OCPP 1.6");
    }
    #endif
    #if MO_ENABLE_V201
    if (context->getOcppVersion() == MO_OCPP_V201) {
        auto rstSvc = context->getModel201().getResetService();
        if (!rstSvc) {
            MO_DBG_ERR("init failure");
            return;
        }
        rstSvc->setExecuteReset(evseId, onResetExecute2, userData);
    }
    #endif
}
#endif //MO_ENABLE_V201

void mo_setDebugCb(void (*debugCb)(const char *msg)) {
    if (!g_context) {
        MO_DBG_ERR("OCPP uninitialized"); //need to call mocpp_initialize before
        return;
    }
    auto context = g_context;

    context->setDebugCb(debugCb);
}

void mo_setDebugCb2(MO_Context *ctx, void (*debugCb2)(int lvl, const char *fn, int line, const char *msg)) {
    if (!ctx) {
        MO_DBG_ERR("OCPP uninitialized"); //need to call mocpp_initialize before
        return;
    }
    auto context = mo_getContext2(ctx);

    context->setDebugCb2(debugCb2);
}

void mo_setTicksCb(uint32_t (*ticksCb)()) {
    mo_setTicksCb2(mo_getApiContext(), ticksCb);
}

void mo_setTicksCb2(MO_Context *ctx, uint32_t (*ticksCb)()) {
    if (!ctx) {
        MO_DBG_ERR("OCPP uninitialized"); //need to call mocpp_initialize before
        return;
    }
    auto context = mo_getContext2(ctx);

    context->setTicksCb(ticksCb);
}

void mo_setRngCb(uint32_t (*rngCb)()) {
    mo_setRngCb2(mo_getApiContext(), rngCb);
}

void mo_setRngCb2(MO_Context *ctx, uint32_t (*rngCb)()) {
    if (!ctx) {
        MO_DBG_ERR("OCPP uninitialized"); //need to call mocpp_initialize before
        return;
    }
    auto context = mo_getContext2(ctx);

    context->setRngCb(rngCb);
}

MO_FilesystemAdapter *mo_getFilesystem() {
    return mo_getFilesystem2(mo_getApiContext());
}

MO_FilesystemAdapter *mo_getFilesystem2(MO_Context *ctx) {
    if (!ctx) {
        MO_DBG_ERR("OCPP uninitialized"); //need to call mocpp_initialize before
        return nullptr;
    }
    auto context = mo_getContext2(ctx);

    return context->getFilesystem();
}

#if MO_ENABLE_MBEDTLS
void mo_setFtpConfig2(MO_Context *ctx, MO_FTPConfig ftpConfig) {
    if (!ctx) {
        MO_DBG_ERR("OCPP uninitialized"); //need to call mocpp_initialize before
        return;
    }
    auto context = mo_getContext2(ctx);

    context->setFtpConfig(ftpConfig);
}

#if MO_ENABLE_DIAGNOSTICS
void mo_setDiagnosticsReader(MO_Context *ctx, size_t (*readBytes)(char*, size_t, void*), void(*onClose)(void*), void *userData) {
    if (!ctx) {
        MO_DBG_ERR("OCPP uninitialized"); //need to call mocpp_initialize before
        return;
    }
    auto context = mo_getContext2(ctx);

    MicroOcpp::DiagnosticsService *diagSvc = context->getModelCommon().getDiagnosticsService();
    if (!diagSvc) {
        MO_DBG_ERR("init failure");
        return;
    }

    diagSvc->setDiagnosticsReader(readBytes, onClose, userData);
}

void mo_setDiagnosticsFtpServerCert(MO_Context *ctx, const char *cert) {
    if (!ctx) {
        MO_DBG_ERR("OCPP uninitialized"); //need to call mocpp_initialize before
        return;
    }
    auto context = mo_getContext2(ctx);

    MicroOcpp::DiagnosticsService *diagSvc = context->getModelCommon().getDiagnosticsService();
    if (!diagSvc) {
        MO_DBG_ERR("init failure");
        return;
    }

    diagSvc->setFtpServerCert(cert);
}
#endif //MO_ENABLE_DIAGNOSTICS

#if MO_ENABLE_V16 && MO_ENABLE_FIRMWAREMANAGEMENT
//void mo_setFirmwareFtpServerCert(const char *cert); //zero-copy mode, i.e. cert must outlive MO
#endif //MO_ENABLE_V16 && MO_ENABLE_FIRMWAREMANAGEMENT

#endif //MO_ENABLE_MBEDTLS

/*
 * Transaction management (Advanced)
 */

bool mo_isTransactionAuthorized() {
    return mo_isTransactionAuthorized2(mo_getApiContext(), EVSE_ID_1);
}

bool mo_isTransactionAuthorized2(MO_Context *ctx, unsigned int evseId) {
    if (!ctx) {
        MO_DBG_ERR("OCPP uninitialized"); //need to call mocpp_initialize before
        return false;
    }
    auto context = mo_getContext2(ctx);

    bool res = false;

    #if MO_ENABLE_V16
    if (context->getOcppVersion() == MO_OCPP_V16) {
        auto txSvc = context->getModel16().getTransactionService();
        auto txSvcEvse = txSvc ? txSvc->getEvse(evseId) : nullptr;
        if (!txSvcEvse) {
            MO_DBG_ERR("init failure");
            return false;
        }
        auto tx = txSvcEvse->getTransaction();
        res = (tx && tx->isAuthorized());
    }
    #endif
    #if MO_ENABLE_V201
    if (context->getOcppVersion() == MO_OCPP_V201) {
        auto txSvc = context->getModel201().getTransactionService();
        auto txSvcEvse = txSvc ? txSvc->getEvse(evseId) : nullptr;
        if (!txSvcEvse) {
            MO_DBG_ERR("init failure");
            return false;
        }
        auto tx = txSvcEvse->getTransaction();
        res = (tx && tx->isAuthorized);
    }
    #endif

    return res;
}

//Returns if server has rejected transaction in StartTx or TransactionEvent response
bool mo_isTransactionDeauthorized() {
    return mo_isTransactionDeauthorized2(mo_getApiContext(), EVSE_ID_1);
}

bool mo_isTransactionDeauthorized2(MO_Context *ctx, unsigned int evseId) {
    if (!ctx) {
        MO_DBG_ERR("OCPP uninitialized"); //need to call mocpp_initialize before
        return false;
    }
    auto context = mo_getContext2(ctx);

    bool res = false;

    #if MO_ENABLE_V16
    if (context->getOcppVersion() == MO_OCPP_V16) {
        auto txSvc = context->getModel16().getTransactionService();
        auto txSvcEvse = txSvc ? txSvc->getEvse(evseId) : nullptr;
        if (!txSvcEvse) {
            MO_DBG_ERR("init failure");
            return false;
        }
        auto tx = txSvcEvse->getTransaction();
        res = (tx && tx->isIdTagDeauthorized());
    }
    #endif
    #if MO_ENABLE_V201
    if (context->getOcppVersion() == MO_OCPP_V201) {
        auto txSvc = context->getModel201().getTransactionService();
        auto txSvcEvse = txSvc ? txSvc->getEvse(evseId) : nullptr;
        if (!txSvcEvse) {
            MO_DBG_ERR("init failure");
            return false;
        }
        auto tx = txSvcEvse->getTransaction();
        res = (tx && tx->isDeauthorized);
    }
    #endif

    return res;
}

//Returns start unix time of transaction. Returns 0 if unix time is not known
int32_t mo_getTransactionStartUnixTime() {
    return mo_getTransactionStartUnixTime2(mo_getApiContext(), EVSE_ID_1);
}

int32_t mo_getTransactionStartUnixTime2(MO_Context *ctx, unsigned int evseId) {
    if (!ctx) {
        MO_DBG_ERR("OCPP uninitialized"); //need to call mocpp_initialize before
        return 0;
    }
    auto context = mo_getContext2(ctx);

    MicroOcpp::Timestamp startTime;

    #if MO_ENABLE_V16
    if (context->getOcppVersion() == MO_OCPP_V16) {
        auto txSvc = context->getModel16().getTransactionService();
        auto txSvcEvse = txSvc ? txSvc->getEvse(evseId) : nullptr;
        if (!txSvcEvse) {
            MO_DBG_ERR("init failure");
            return 0;
        }
        if (auto tx = txSvcEvse->getTransaction()) {
            startTime = tx->getStartTimestamp();
        }
    }
    #endif
    #if MO_ENABLE_V201
    if (context->getOcppVersion() == MO_OCPP_V201) {
        auto txSvc = context->getModel201().getTransactionService();
        auto txSvcEvse = txSvc ? txSvc->getEvse(evseId) : nullptr;
        if (!txSvcEvse) {
            MO_DBG_ERR("init failure");
            return 0;
        }
        if (auto tx = txSvcEvse->getTransaction()) {
            startTime = tx->startTimestamp;
        }
    }
    #endif

    int32_t res = 0;

    if (!context->getClock().toUnixTime(startTime, res)) {
        MO_DBG_ERR("cannot determine transaction start unix time");
        res = 0;
    }

    return res;
}

#if MO_ENABLE_V16
//Overrides start unix time of transaction
void mo_setTransactionStartUnixTime(int32_t unixTime) {
    mo_setTransactionStartUnixTime2(mo_getApiContext(), EVSE_ID_1, unixTime);
}

void mo_setTransactionStartUnixTime2(MO_Context *ctx, unsigned int evseId, int32_t unixTime) {
    if (!ctx) {
        MO_DBG_ERR("OCPP uninitialized"); //need to call mocpp_initialize before
        return;
    }
    auto context = mo_getContext2(ctx);

    #if MO_ENABLE_V16
    if (context->getOcppVersion() == MO_OCPP_V16) {
        auto txSvc = context->getModel16().getTransactionService();
        auto txSvcEvse = txSvc ? txSvc->getEvse(evseId) : nullptr;
        if (!txSvcEvse) {
            MO_DBG_ERR("init failure");
            return;
        }
        if (auto tx = txSvcEvse->getTransaction()) {

            MicroOcpp::Timestamp t;
            if (!context->getClock().fromUnixTime(t, unixTime)) {
                MO_DBG_ERR("invalid unix time");
                return;
            }
            tx->setStartTimestamp(t);
        }
    }
    #endif
    #if MO_ENABLE_V201
    if (context->getOcppVersion() == MO_OCPP_V201) {
        MO_DBG_ERR("mo_setTransactionStartUnixTime not supported with OCPP 2.0.1");
    }
    #endif
}

int32_t mo_getTransactionStopUnixTime() {
    return mo_getTransactionStopUnixTime2(mo_getApiContext(), EVSE_ID_1);
}

int32_t mo_getTransactionStopUnixTime2(MO_Context *ctx, unsigned int evseId) {
    if (!ctx) {
        MO_DBG_ERR("OCPP uninitialized"); //need to call mocpp_initialize before
        return 0;
    }
    auto context = mo_getContext2(ctx);

    int32_t res = 0;

    #if MO_ENABLE_V16
    if (context->getOcppVersion() == MO_OCPP_V16) {
        auto txSvc = context->getModel16().getTransactionService();
        auto txSvcEvse = txSvc ? txSvc->getEvse(evseId) : nullptr;
        if (!txSvcEvse) {
            MO_DBG_ERR("init failure");
            return 0;
        }
        if (auto tx = txSvcEvse->getTransaction()) {
            if (!context->getClock().toUnixTime(tx->getStopTimestamp(), res)) {
                MO_DBG_ERR("cannot determine transaction stop unix time");
                res = 0;
            }
        }
    }
    #endif
    #if MO_ENABLE_V201
    if (context->getOcppVersion() == MO_OCPP_V201) {
        MO_DBG_ERR("mo_setTransactionStartUnixTime not supported with OCPP 2.0.1");
    }
    #endif

    return res;
}

//Overrides stop unix time of transaction
void mo_setTransactionStopUnixTime(int32_t unixTime) {
    mo_setTransactionStopUnixTime2(mo_getApiContext(), EVSE_ID_1, unixTime);
}

void mo_setTransactionStopUnixTime2(MO_Context *ctx, unsigned int evseId, int32_t unixTime) {
    if (!ctx) {
        MO_DBG_ERR("OCPP uninitialized"); //need to call mocpp_initialize before
        return;
    }
    auto context = mo_getContext2(ctx);

    #if MO_ENABLE_V16
    if (context->getOcppVersion() == MO_OCPP_V16) {
        auto txSvc = context->getModel16().getTransactionService();
        auto txSvcEvse = txSvc ? txSvc->getEvse(evseId) : nullptr;
        if (!txSvcEvse) {
            MO_DBG_ERR("init failure");
            return;
        }
        if (auto tx = txSvcEvse->getTransaction()) {

            MicroOcpp::Timestamp t;
            if (!context->getClock().fromUnixTime(t, unixTime)) {
                MO_DBG_ERR("invalid unix time");
                return;
            }
            tx->setStopTimestamp(t);
        }
    }
    #endif
    #if MO_ENABLE_V201
    if (context->getOcppVersion() == MO_OCPP_V201) {
        MO_DBG_ERR("mo_setTransactionStopUnixTime not supported with OCPP 2.0.1");
    }
    #endif
}

int32_t mo_getTransactionMeterStart() {
    return mo_getTransactionMeterStart2(mo_getApiContext(), EVSE_ID_1);
}

int32_t mo_getTransactionMeterStart2(MO_Context *ctx, unsigned int evseId) {
    if (!ctx) {
        MO_DBG_ERR("OCPP uninitialized"); //need to call mocpp_initialize before
        return -1;
    }
    auto context = mo_getContext2(ctx);

    int32_t res = -1;

    #if MO_ENABLE_V16
    if (context->getOcppVersion() == MO_OCPP_V16) {
        auto txSvc = context->getModel16().getTransactionService();
        auto txSvcEvse = txSvc ? txSvc->getEvse(evseId) : nullptr;
        if (!txSvcEvse) {
            MO_DBG_ERR("init failure");
            return -1;
        }
        if (auto tx = txSvcEvse->getTransaction()) {
            res = tx->getMeterStart();
        }
    }
    #endif
    #if MO_ENABLE_V201
    if (context->getOcppVersion() == MO_OCPP_V201) {
        MO_DBG_ERR("mo_getTransactionMeterStart not supported with OCPP 2.0.1");
    }
    #endif

    return res;
}

//Overrides meterStart of transaction
void mo_setTransactionMeterStart(int32_t meterStart) {
    mo_setTransactionMeterStart2(mo_getApiContext(), EVSE_ID_1, meterStart);
}

void mo_setTransactionMeterStart2(MO_Context *ctx, unsigned int evseId, int32_t meterStart) {
    if (!ctx) {
        MO_DBG_ERR("OCPP uninitialized"); //need to call mocpp_initialize before
        return;
    }
    auto context = mo_getContext2(ctx);

    #if MO_ENABLE_V16
    if (context->getOcppVersion() == MO_OCPP_V16) {
        auto txSvc = context->getModel16().getTransactionService();
        auto txSvcEvse = txSvc ? txSvc->getEvse(evseId) : nullptr;
        if (!txSvcEvse) {
            MO_DBG_ERR("init failure");
            return;
        }
        if (auto tx = txSvcEvse->getTransaction()) {
            tx->setMeterStart(meterStart);
        }
    }
    #endif
    #if MO_ENABLE_V201
    if (context->getOcppVersion() == MO_OCPP_V201) {
        MO_DBG_ERR("mo_setTransactionMeterStart not supported with OCPP 2.0.1");
    }
    #endif
}

int32_t mo_getTransactionMeterStop() {
    return mo_getTransactionMeterStop2(mo_getApiContext(), EVSE_ID_1);
}

int32_t mo_getTransactionMeterStop2(MO_Context *ctx, unsigned int evseId) {
    if (!ctx) {
        MO_DBG_ERR("OCPP uninitialized"); //need to call mocpp_initialize before
        return -1;
    }
    auto context = mo_getContext2(ctx);

    int32_t res = -1;

    #if MO_ENABLE_V16
    if (context->getOcppVersion() == MO_OCPP_V16) {
        auto txSvc = context->getModel16().getTransactionService();
        auto txSvcEvse = txSvc ? txSvc->getEvse(evseId) : nullptr;
        if (!txSvcEvse) {
            MO_DBG_ERR("init failure");
            return -1;
        }
        if (auto tx = txSvcEvse->getTransaction()) {
            res = tx->getMeterStop();
        }
    }
    #endif
    #if MO_ENABLE_V201
    if (context->getOcppVersion() == MO_OCPP_V201) {
        MO_DBG_ERR("mo_getTransactionMeterStop not supported with OCPP 2.0.1");
    }
    #endif

    return res;
}

//Overrides meterStop of transaction
void mo_setTransactionMeterStop(int32_t meterStop) {
    mo_setTransactionMeterStop2(mo_getApiContext(), EVSE_ID_1, meterStop);
}

void mo_setTransactionMeterStop2(MO_Context *ctx, unsigned int evseId, int32_t meterStop) {
    if (!ctx) {
        MO_DBG_ERR("OCPP uninitialized"); //need to call mocpp_initialize before
        return;
    }
    auto context = mo_getContext2(ctx);

    #if MO_ENABLE_V16
    if (context->getOcppVersion() == MO_OCPP_V16) {
        auto txSvc = context->getModel16().getTransactionService();
        auto txSvcEvse = txSvc ? txSvc->getEvse(evseId) : nullptr;
        if (!txSvcEvse) {
            MO_DBG_ERR("init failure");
            return;
        }
        if (auto tx = txSvcEvse->getTransaction()) {
            tx->setMeterStop(meterStop);
        }
    }
    #endif
    #if MO_ENABLE_V201
    if (context->getOcppVersion() == MO_OCPP_V201) {
        MO_DBG_ERR("mo_setTransactionMeterStop not supported with OCPP 2.0.1");
    }
    #endif
}
#endif //MO_ENABLE_V16

//helper function
namespace MO_Facade {
MicroOcpp::Mutability convertMutability(MO_Mutability mutability) {
    auto res = MicroOcpp::Mutability::None;
    switch (mutability) {
        case MO_Mutability_ReadWrite:
            res = MicroOcpp::Mutability::ReadWrite;
            break;
        case MO_Mutability_ReadOnly:
            res = MicroOcpp::Mutability::ReadOnly;
            break;
        case MO_Mutability_WriteOnly:
            res = MicroOcpp::Mutability::WriteOnly;
            break;
        case MO_Mutability_None:
            res = MicroOcpp::Mutability::None;
            break;
    }
    return res;
}
} //namespace MO_Facade

using namespace MO_Facade;

#if MO_ENABLE_V16
bool mo_declareConfigurationInt(const char *key, int factoryDefault, MO_Mutability mutability, bool persistent, bool rebootRequired) {
    return mo_declareVarConfigInt(mo_getApiContext(), NULL, NULL, key, factoryDefault, mutability, persistent, rebootRequired);
}

bool mo_declareConfigurationBool(const char *key, bool factoryDefault, MO_Mutability mutability, bool persistent, bool rebootRequired) {
    return mo_declareVarConfigBool(mo_getApiContext(), NULL, NULL, key, factoryDefault, mutability, persistent, rebootRequired);
}

bool mo_declareConfigurationString(const char *key, const char *factoryDefault, MO_Mutability mutability, bool persistent, bool rebootRequired) {
    return mo_declareVarConfigString(mo_getApiContext(), NULL, NULL, key, factoryDefault, mutability, persistent, rebootRequired);
}

//Set Config value. Set after `mo_setup()`
bool mo_setConfigurationInt(const char *key, int value) {
    return mo_setVarConfigInt(mo_getApiContext(), NULL, NULL, key, value);
}
bool mo_setConfigurationBool(const char *key, bool value) {
    return mo_setVarConfigBool(mo_getApiContext(), NULL, NULL, key, value);
}
bool mo_setConfigurationString(const char *key, const char *value) {
    return mo_setVarConfigString(mo_getApiContext(), NULL, NULL, key, value);
}

//Get Config value. MO writes the value into `valueOut`. Call after `mo_setup()`
bool mo_getConfigurationInt(const char *key, int *valueOut) {
    return mo_getVarConfigInt(mo_getApiContext(), NULL, NULL, key, valueOut);
}
bool mo_getConfigurationBool(const char *key, bool *valueOut) {
    return mo_getVarConfigBool(mo_getApiContext(), NULL, NULL, key, valueOut);
}
bool mo_getConfigurationString(const char *key, const char **valueOut) {
    return mo_getVarConfigString(mo_getApiContext(), NULL, NULL, key, valueOut);
}
#endif //MO_ENABLE_V16

bool mo_declareVarConfigInt(MO_Context *ctx, const char *component201, const char *name201, const char *key16, int factoryDefault, MO_Mutability mutability, bool persistent, bool rebootRequired) {
    if (!ctx) {
        MO_DBG_ERR("OCPP uninitialized"); //need to call mocpp_initialize before
        return false;
    }
    auto context = mo_getContext2(ctx);

    bool success = false;

    #if MO_ENABLE_V16
    if (context->getOcppVersion() == MO_OCPP_V16) {
        if (!key16) {
            //disabled OCPP 1.6 compatibility
            return true;
        }
        auto configSvc = context->getModel16().getConfigurationService();
        if (!configSvc) {
            MO_DBG_ERR("setup failure");
            return false;
        }

        const char *containerId = persistent ? CONFIGS_FN_FACADE : MO_CONFIGURATION_VOLATILE;
        success = configSvc->declareConfiguration<int>(key16, factoryDefault, containerId, convertMutability(mutability), rebootRequired);
    }
    #endif
    #if MO_ENABLE_V201
    if (context->getOcppVersion() == MO_OCPP_V201) {
        if (!component201 || !name201) {
            //disabled OCPP 2.0.1 compatibility
            return true;
        }
        auto varSvc = context->getModel201().getVariableService();
        if (!varSvc) {
            MO_DBG_ERR("setup failure");
            return false;
        }

        success = varSvc->declareVariable<int>(component201, name201, factoryDefault, convertMutability(mutability), persistent, MicroOcpp::v201::Variable::AttributeTypeSet(), rebootRequired);
    }
    #endif

    return success;
}
bool mo_declareVarConfigBool(MO_Context *ctx, const char *component201, const char *name201, const char *key16, bool factoryDefault, MO_Mutability mutability, bool persistent, bool rebootRequired) {
    if (!ctx) {
        MO_DBG_ERR("OCPP uninitialized"); //need to call mocpp_initialize before
        return false;
    }
    auto context = mo_getContext2(ctx);

    bool success = false;

    #if MO_ENABLE_V16
    if (context->getOcppVersion() == MO_OCPP_V16) {
        if (!key16) {
            //disabled OCPP 1.6 compatibility
            return true;
        }
        auto configSvc = context->getModel16().getConfigurationService();
        if (!configSvc) {
            MO_DBG_ERR("setup failure");
            return false;
        }

        const char *containerId = persistent ? CONFIGS_FN_FACADE : MO_CONFIGURATION_VOLATILE;
        success = configSvc->declareConfiguration<bool>(key16, factoryDefault, containerId, convertMutability(mutability), rebootRequired);
    }
    #endif
    #if MO_ENABLE_V201
    if (context->getOcppVersion() == MO_OCPP_V201) {
        if (!component201 || !name201) {
            //disabled OCPP 2.0.1 compatibility
            return true;
        }
        auto varSvc = context->getModel201().getVariableService();
        if (!varSvc) {
            MO_DBG_ERR("setup failure");
            return false;
        }

        success = varSvc->declareVariable<bool>(component201, name201, factoryDefault, convertMutability(mutability), persistent, MicroOcpp::v201::Variable::AttributeTypeSet(), rebootRequired);
    }
    #endif

    return success;
}
bool mo_declareVarConfigString(MO_Context *ctx, const char *component201, const char *name201, const char *key16, const char *factoryDefault, MO_Mutability mutability, bool persistent, bool rebootRequired) {
    if (!ctx) {
        MO_DBG_ERR("OCPP uninitialized"); //need to call mocpp_initialize before
        return false;
    }
    auto context = mo_getContext2(ctx);

    bool success = false;

    #if MO_ENABLE_V16
    if (context->getOcppVersion() == MO_OCPP_V16) {
        if (!key16) {
            //disabled OCPP 1.6 compatibility
            return true;
        }
        auto configSvc = context->getModel16().getConfigurationService();
        if (!configSvc) {
            MO_DBG_ERR("setup failure");
            return false;
        }

        const char *containerId = persistent ? CONFIGS_FN_FACADE : MO_CONFIGURATION_VOLATILE;
        success = configSvc->declareConfiguration<const char*>(key16, factoryDefault, containerId, convertMutability(mutability), rebootRequired);
    }
    #endif
    #if MO_ENABLE_V201
    if (context->getOcppVersion() == MO_OCPP_V201) {
        if (!component201 || !name201) {
            //disabled OCPP 2.0.1 compatibility
            return true;
        }
        auto varSvc = context->getModel201().getVariableService();
        if (!varSvc) {
            MO_DBG_ERR("setup failure");
            return false;
        }

        success = varSvc->declareVariable<const char*>(component201, name201, factoryDefault, convertMutability(mutability), persistent, MicroOcpp::v201::Variable::AttributeTypeSet(), rebootRequired);
    }
    #endif

    return success;
}

//Set Variable or Config value. Set after `mo_setup()`. If running OCPP 2.0.1, `key16` can be NULL
bool mo_setVarConfigInt(MO_Context *ctx, const char *component201, const char *name201, const char *key16, int value) {
    if (!ctx) {
        MO_DBG_ERR("OCPP uninitialized"); //need to call mocpp_initialize before
        return false;
    }
    auto context = mo_getContext2(ctx);

    bool success = false;

    #if MO_ENABLE_V16
    if (context->getOcppVersion() == MO_OCPP_V16) {
        if (!key16) {
            //disabled OCPP 1.6 compatibility
            return true;
        }
        auto configSvc = context->getModel16().getConfigurationService();
        if (!configSvc) {
            MO_DBG_ERR("setup failure");
            return false;
        }
        auto config = configSvc->getConfiguration(key16);
        if (!config) {
            MO_DBG_ERR("could not find config %s", key16);
            return false;
        }
        config->setInt(value);
        success = configSvc->commit();
    }
    #endif
    #if MO_ENABLE_V201
    if (context->getOcppVersion() == MO_OCPP_V201) {
        if (!component201 || !name201) {
            //disabled OCPP 2.0.1 compatibility
            return true;
        }
        auto varSvc = context->getModel201().getVariableService();
        if (!varSvc) {
            MO_DBG_ERR("setup failure");
            return false;
        }
        auto variable = varSvc->getVariable(component201, name201);
        if (!variable) {
            MO_DBG_ERR("could not find variable %s %s", component201, name201);
            return false;
        }
        variable->setInt(value);
        success = varSvc->commit();
    }
    #endif

    return success;
}
bool mo_setVarConfigBool(MO_Context *ctx, const char *component201, const char *name201, const char *key16, bool value) {
    if (!ctx) {
        MO_DBG_ERR("OCPP uninitialized"); //need to call mocpp_initialize before
        return false;
    }
    auto context = mo_getContext2(ctx);

    bool success = false;

    #if MO_ENABLE_V16
    if (context->getOcppVersion() == MO_OCPP_V16) {
        if (!key16) {
            //disabled OCPP 1.6 compatibility
            return true;
        }

        auto configSvc = context->getModel16().getConfigurationService();
        if (!configSvc) {
            MO_DBG_ERR("setup failure");
            return false;
        }
        auto config = configSvc->getConfiguration(key16);
        if (!config) {
            MO_DBG_ERR("could not find config %s", key16);
            return false;
        }
        config->setBool(value);
        success = configSvc->commit();
    }
    #endif
    #if MO_ENABLE_V201
    if (context->getOcppVersion() == MO_OCPP_V201) {
        if (!component201 || !name201) {
            //disabled OCPP 2.0.1 compatibility
            return true;
        }
        auto varSvc = context->getModel201().getVariableService();
        if (!varSvc) {
            MO_DBG_ERR("setup failure");
            return false;
        }
        auto variable = varSvc->getVariable(component201, name201);
        if (!variable) {
            MO_DBG_ERR("could not find variable %s %s", component201, name201);
            return false;
        }
        variable->setBool(value);
        success = varSvc->commit();
    }
    #endif

    return success;
}

bool mo_setVarConfigString(MO_Context *ctx, const char *component201, const char *name201, const char *key16, const char *value) {
    if (!ctx) {
        MO_DBG_ERR("OCPP uninitialized"); //need to call mocpp_initialize before
        return false;
    }
    auto context = mo_getContext2(ctx);

    bool success = false;

    #if MO_ENABLE_V16
    if (context->getOcppVersion() == MO_OCPP_V16) {
        if (!key16) {
            //disabled OCPP 1.6 compatibility
            return true;
        }
        auto configSvc = context->getModel16().getConfigurationService();
        if (!configSvc) {
            MO_DBG_ERR("setup failure");
            return false;
        }
        auto config = configSvc->getConfiguration(key16);
        if (!config) {
            MO_DBG_ERR("could not find config %s", key16);
            return false;
        }
        success = config->setString(value);
        if (success) {
            success = configSvc->commit();
        }
    }
    #endif
    #if MO_ENABLE_V201
    if (context->getOcppVersion() == MO_OCPP_V201) {
        if (!component201 || !name201) {
            //disabled OCPP 2.0.1 compatibility
            return true;
        }
        auto varSvc = context->getModel201().getVariableService();
        if (!varSvc) {
            MO_DBG_ERR("setup failure");
            return false;
        }
        auto variable = varSvc->getVariable(component201, name201);
        if (!variable) {
            MO_DBG_ERR("could not find variable %s %s", component201, name201);
            return false;
        }
        success = variable->setString(value);
        if (success) {
            success = varSvc->commit();
        }
    }
    #endif

    return success;
}

bool mo_getVarConfigInt(MO_Context *ctx, const char *component201, const char *name201, const char *key16, int *valueOut) {
    if (!ctx) {
        MO_DBG_ERR("OCPP uninitialized"); //need to call mocpp_initialize before
        return false;
    }
    auto context = mo_getContext2(ctx);

    bool success = false;

    #if MO_ENABLE_V16
    if (context->getOcppVersion() == MO_OCPP_V16) {
        if (!key16) {
            //disabled OCPP 1.6 compatibility
            return true;
        }
        auto configSvc = context->getModel16().getConfigurationService();
        if (!configSvc) {
            MO_DBG_ERR("setup failure");
            return false;
        }
        auto config = configSvc->getConfiguration(key16);
        if (!config) {
            MO_DBG_ERR("could not find config %s", key16);
            return false;
        }
        *valueOut = config->getInt();
        success = true;
    }
    #endif
    #if MO_ENABLE_V201
    if (context->getOcppVersion() == MO_OCPP_V201) {
        if (!component201 || !name201) {
            //disabled OCPP 2.0.1 compatibility
            return true;
        }
        auto varSvc = context->getModel201().getVariableService();
        if (!varSvc) {
            MO_DBG_ERR("setup failure");
            return false;
        }
        auto variable = varSvc->getVariable(component201, name201);
        if (!variable) {
            MO_DBG_ERR("could not find variable %s %s", component201, name201);
            return false;
        }
        *valueOut = variable->getInt();
        success = true;
    }
    #endif

    return success;
}
bool mo_getVarConfigBool(MO_Context *ctx, const char *component201, const char *name201, const char *key16, bool *valueOut) {
    if (!ctx) {
        MO_DBG_ERR("OCPP uninitialized"); //need to call mocpp_initialize before
        return false;
    }
    auto context = mo_getContext2(ctx);

    bool success = false;

    #if MO_ENABLE_V16
    if (context->getOcppVersion() == MO_OCPP_V16) {
        if (!key16) {
            //disabled OCPP 1.6 compatibility
            return true;
        }
        auto configSvc = context->getModel16().getConfigurationService();
        if (!configSvc) {
            MO_DBG_ERR("setup failure");
            return false;
        }
        auto config = configSvc->getConfiguration(key16);
        if (!config) {
            MO_DBG_ERR("could not find config %s", key16);
            return false;
        }
        *valueOut = config->getBool();
        success = true;
    }
    #endif
    #if MO_ENABLE_V201
    if (context->getOcppVersion() == MO_OCPP_V201) {
        if (!component201 || !name201) {
            //disabled OCPP 2.0.1 compatibility
            return true;
        }
        auto varSvc = context->getModel201().getVariableService();
        if (!varSvc) {
            MO_DBG_ERR("setup failure");
            return false;
        }
        auto variable = varSvc->getVariable(component201, name201);
        if (!variable) {
            MO_DBG_ERR("could not find variable %s %s", component201, name201);
            return false;
        }
        *valueOut = variable->getBool();
        success = true;
    }
    #endif

    return success;
}
bool mo_getVarConfigString(MO_Context *ctx, const char *component201, const char *name201, const char *key16, const char **valueOut) {
    if (!ctx) {
        MO_DBG_ERR("OCPP uninitialized"); //need to call mocpp_initialize before
        return false;
    }
    auto context = mo_getContext2(ctx);

    bool success = false;

    #if MO_ENABLE_V16
    if (context->getOcppVersion() == MO_OCPP_V16) {
        if (!key16) {
            //disabled OCPP 1.6 compatibility
            return true;
        }
        auto configSvc = context->getModel16().getConfigurationService();
        if (!configSvc) {
            MO_DBG_ERR("setup failure");
            return false;
        }
        auto config = configSvc->getConfiguration(key16);
        if (!config) {
            MO_DBG_ERR("could not find config %s", key16);
            return false;
        }
        *valueOut = config->getString();
        success = true;
    }
    #endif
    #if MO_ENABLE_V201
    if (context->getOcppVersion() == MO_OCPP_V201) {
        if (!component201 || !name201) {
            //disabled OCPP 2.0.1 compatibility
            return true;
        }
        auto varSvc = context->getModel201().getVariableService();
        if (!varSvc) {
            MO_DBG_ERR("setup failure");
            return false;
        }
        auto variable = varSvc->getVariable(component201, name201);
        if (!variable) {
            MO_DBG_ERR("could not find variable %s %s", component201, name201);
            return false;
        }
        *valueOut = variable->getString();
        success = true;
    }
    #endif

    return success;
}

//bool mo_addCustomVarConfigInt(const char *component201, const char *name201, const char *key16, int (*getValue)(const char*, const char*, const char*, void*), void (*setValue)(int, const char*, const char*, const char*, void*), void *userData);
//bool mo_addCustomVarConfigBool(const char *component201, const char *name201, const char *key16, bool (*getValue)(const char*, const char*, const char*, void*), void (*setValue)(bool, const char*, const char*, const char*, void*), void *userData);
//bool mo_addCustomVarConfigString(const char *component201, const char *name201, const char *key16, const char* (*getValue)(const char*, const char*, const char*, void*), bool (*setValue)(const char*, const char*, const char*, const char*, void*), void *userData);

MicroOcpp::Context *mo_getContext() {
    return g_context;
}

MicroOcpp::Context *mo_getContext2(MO_Context *ctx) {
    return reinterpret_cast<MicroOcpp::Context*>(ctx);
}

MO_Context *mo_initialize2() {
    auto context = new MicroOcpp::Context();
    if (!context) {
        MO_DBG_ERR("OOM");
        return nullptr;
    }
    return reinterpret_cast<MO_Context*>(context);
}

bool mo_setup2(MO_Context *ctx) {
    if (!ctx) {
        MO_DBG_ERR("OCPP uninitialized"); //need to call mocpp_initialize before
        return false;
    }
    auto context = mo_getContext2(ctx);

    return context->setup();
}

void mo_deinitialize2(MO_Context *ctx) {
    if (!ctx) {
        MO_DBG_ERR("OCPP uninitialized"); //need to call mocpp_initialize before
        return;
    }
    auto context = mo_getContext2(ctx);

    delete context;
}

void mo_loop2(MO_Context *ctx) {
    if (!ctx) {
        MO_DBG_ERR("OCPP uninitialized"); //need to call mocpp_initialize before
        return;
    }
    auto context = mo_getContext2(ctx);

    context->loop();
}

//Send operation manually and bypass MO business logic
bool mo_sendRequest(MO_Context *ctx, const char *operationType,
        const char *payloadJson,
        void (*onResponse)(const char *payloadJson, void *userData),
        void (*onAbort)(void *userData),
        void *userData) {

    MicroOcpp::Operation *operation = nullptr;
    std::unique_ptr<MicroOcpp::Request> request;

    MicroOcpp::Context *context = nullptr;
    if (!ctx) {
        MO_DBG_ERR("OCPP uninitialized"); //need to call mocpp_initialize before
        goto fail;
    }
    context = mo_getContext2(ctx);

    if (auto customOperation = new MicroOcpp::CustomOperation()) {

        if (!customOperation->setupEvseInitiated(operationType, payloadJson, onResponse, userData)) {
            MO_DBG_ERR("create operation failure");
            delete customOperation;
            goto fail;
        }

        operation = static_cast<MicroOcpp::Operation*>(customOperation);
    } else {
        MO_DBG_ERR("OOM");
        goto fail;
    }

    request = MicroOcpp::makeRequest(*context, operation);
    if (!request) {
        MO_DBG_ERR("OOM");
        goto fail;
    }

    if (onAbort) {
        request->setOnAbort([onAbort, userData] () {
            onAbort(userData);
        });
    }

    request->setTimeout(20);

    context->getMessageService().sendRequest(std::move(request));
    return true;
fail:
    delete operation;
    if (onAbort) {
        //onAbort execution is guaranteed to allow client to free up resources here
        onAbort(userData);
    }
    return false;
}

//Set custom operation handler for incoming reqeusts and bypass MO business logic
bool mo_setRequestHandler(MO_Context *ctx, const char *operationType,
        void (*onRequest)(const char *operationType, const char *payloadJson, void **userStatus, void *userData),
        int (*writeResponse)(const char *operationType, char *buf, size_t size, void *userStatus, void *userData),
        void (*finally)(const char *operationType, void *userStatus, void *userData),
        void *userData) {

    if (!ctx) {
        MO_DBG_ERR("OCPP uninitialized"); //need to call mocpp_initialize before
        return false;
    }
    auto context = mo_getContext2(ctx);

    bool success = context->getMessageService().registerOperation(operationType, onRequest, writeResponse, finally, userData);
    if (!success) {
        MO_DBG_ERR("could not register operation %s", operationType);
    }
    return success;
}

//Sniff incoming requests without control over the response
bool mo_setOnReceiveRequest(MO_Context *ctx, const char *operationType,
        void (*onRequest)(const char *operationType, const char *payloadJson, void *userData),
        void *userData) {

    if (!ctx) {
        MO_DBG_ERR("OCPP uninitialized"); //need to call mocpp_initialize before
        return false;
    }
    auto context = mo_getContext2(ctx);

    bool success = context->getMessageService().setOnReceiveRequest(operationType, onRequest, userData);
    if (!success) {
        MO_DBG_ERR("could not set onRequest %s", operationType);
    }
    return success;
}

//Sniff outgoing responses without control over the response
bool mo_setOnSendConf(MO_Context *ctx, const char *operationType,
        void (*onSendConf)(const char *operationType, const char *payloadJson, void *userData),
        void *userData) {

    if (!ctx) {
        MO_DBG_ERR("OCPP uninitialized"); //need to call mocpp_initialize before
        return false;
    }
    auto context = mo_getContext2(ctx);

    bool success = context->getMessageService().setOnSendConf(operationType, onSendConf, userData);
    if (!success) {
        MO_DBG_ERR("could not set onRequest %s", operationType);
    }
    return success;
}
