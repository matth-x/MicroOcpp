// matth-x/MicroOcpp
// Copyright Matthias Akstaller 2019 - 2025
// MIT License

#include <MicroOcpp/Model/Boot/BootService.h>

#include <limits>

#include <MicroOcpp/Context.h>
#include <MicroOcpp/Model/Model.h>
#include <MicroOcpp/Model/Configuration/ConfigurationService.h>
#include <MicroOcpp/Model/Variables/VariableService.h>
#include <MicroOcpp/Model/RemoteControl/RemoteControlService.h>
#include <MicroOcpp/Core/Request.h>
#include <MicroOcpp/Core/FilesystemUtils.h>
#include <MicroOcpp/Core/PersistencyUtils.h>
#include <MicroOcpp/Operations/BootNotification.h>
#include <MicroOcpp/Platform.h>
#include <MicroOcpp/Debug.h>

#if MO_ENABLE_V16 || MO_ENABLE_V201

using namespace MicroOcpp;

void mo_BootNotificationData_init(MO_BootNotificationData *bnData) {
    memset(bnData, 0, sizeof(*bnData));
}

unsigned int PreBootQueue::getFrontRequestOpNr() {
    if (!activatedPostBootCommunication) {
        return 0;
    }

    return VolatileRequestQueue::getFrontRequestOpNr();
}

void PreBootQueue::activatePostBootCommunication() {
    activatedPostBootCommunication = true;
}

RegistrationStatus MicroOcpp::deserializeRegistrationStatus(const char *serialized) {
    if (!strcmp(serialized, "Accepted")) {
        return RegistrationStatus::Accepted;
    } else if (!strcmp(serialized, "Pending")) {
        return RegistrationStatus::Pending;
    } else if (!strcmp(serialized, "Rejected")) {
        return RegistrationStatus::Rejected;
    } else {
        MO_DBG_ERR("deserialization error");
        return RegistrationStatus::UNDEFINED;
    }
}

BootService::BootService(Context& context) : MemoryManaged("v16/v201.Boot.BootService"), context(context) {

}

BootService::~BootService() {
    MO_FREE(bnDataBuf);
    bnDataBuf = nullptr;
}

bool BootService::setup() {

    context.getMessageService().setPreBootSendQueue(&preBootQueue); //register PreBootQueue in RequestQueue module

    filesystem = context.getFilesystem();

    ocppVersion = context.getOcppVersion();

    #if MO_ENABLE_V16
    if (ocppVersion == MO_OCPP_V16) {

        auto configService = context.getModel16().getConfigurationService();
        if (!configService) {
            MO_DBG_ERR("initialization error");
            return false;
        }

        //if transactions can start before the BootNotification succeeds
        preBootTransactionsBoolV16 = configService->declareConfiguration<bool>(MO_CONFIG_EXT_PREFIX "PreBootTransactions", false);
        if (!preBootTransactionsBoolV16) {
            MO_DBG_ERR("initialization error");
            return false;
        }

        heartbeatService = context.getModel16().getHeartbeatService(); //optional

        RemoteControlService *rcService = context.getModel16().getRemoteControlService();
        if (!rcService) {
            MO_DBG_ERR("initialization error");
            return false;
        }

        rcService->addTriggerMessageHandler("BootNotification", [] (Context& context) -> Operation* {
            auto& model = context.getModel16();
            auto& bootSvc = *model.getBootService();
            return new BootNotification(context, bootSvc, model.getHeartbeatService(), bootSvc.getBootNotificationData());
        });
    }
    #endif //MO_ENABLE_V16
    #if MO_ENABLE_V201
    if (ocppVersion == MO_OCPP_V201) {

        auto varService = context.getModel201().getVariableService();
        if (!varService) {
            MO_DBG_ERR("initialization error");
            return false;
        }

        //if transactions can start before the BootNotification succeeds
        preBootTransactionsBoolV201 = varService->declareVariable<bool>("CustomizationCtrlr", "PreBootTransactions", false);
        if (!preBootTransactionsBoolV201) {
            MO_DBG_ERR("initialization error");
            return false;
        }

        heartbeatService = context.getModel201().getHeartbeatService(); //optional

        RemoteControlService *rcService = context.getModel16().getRemoteControlService();
        if (!rcService) {
            MO_DBG_ERR("initialization error");
            return false;
        }

        rcService->addTriggerMessageHandler("BootNotification", [] (Context& context, int evseId) -> TriggerMessageStatus {
            (void)evseId;
            auto& model = context.getModel201();
            auto& bootSvc = *model.getBootService();

            if (bootSvc.getRegistrationStatus() == RegistrationStatus::Accepted) {
                //F06.FR.17: if previous BootNotification was accepted, don't allow triggering a new one
                return TriggerMessageStatus::Rejected;
            } else {
                auto operation = new BootNotification(context, bootSvc, model.getHeartbeatService(), bootSvc.getBootNotificationData());
                if (!operation) {
                    MO_DBG_ERR("OOM");
                    return TriggerMessageStatus::ERR_INTERNAL;
                }
                auto request = makeRequest(context, operation);
                if (!request) {
                    MO_DBG_ERR("OOM");
                    return TriggerMessageStatus::ERR_INTERNAL;
                }
                if (!context.getMessageService().sendRequestPreBoot(std::move(request))) {
                    MO_DBG_ERR("OOM");
                    return TriggerMessageStatus::ERR_INTERNAL;
                }
                return TriggerMessageStatus::Accepted;
            }
        });
    }
    #endif //MO_ENABLE_V201

    #if MO_ENABLE_MOCK_SERVER
    context.getMessageService().registerOperation("BootNotification", nullptr, BootNotification::writeMockConf, reinterpret_cast<void*>(&context));
    #endif //MO_ENABLE_MOCK_SERVER

    return true;
}

void BootService::loop() {

    auto& clock = context.getClock();

    if (!executedFirstTime) {
        executedFirstTime = true;
        firstExecutionTimestamp = clock.getUptime();
    }

    int32_t dtFirstExecution;
    if (!clock.delta(clock.getUptime(), firstExecutionTimestamp, dtFirstExecution)) {
        dtFirstExecution = 0;
    }

    if (!executedLongTime && dtFirstExecution >= MO_BOOTSTATS_LONGTIME_MS) {
        executedLongTime = true;
        MO_DBG_DEBUG("boot success timer reached");

        if (filesystem) {
            PersistencyUtils::setBootSuccess(filesystem);
        }
    }

    preBootQueue.loop();

    if (!activatedPostBootCommunication && status == RegistrationStatus::Accepted) {
        preBootQueue.activatePostBootCommunication();
        activatedPostBootCommunication = true;
    }

    bool preBootTransactions = false;
    #if MO_ENABLE_V16
    if (ocppVersion == MO_OCPP_V16) {
        preBootTransactions = preBootTransactionsBoolV16->getBool();
    }
    #endif //MO_ENABLE_V16
    #if MO_ENABLE_V201
    if (ocppVersion == MO_OCPP_V201) {
        preBootTransactions = preBootTransactionsBoolV201->getBool();
    }
    #endif //MO_ENABLE_V201

    if (!activatedModel && (status == RegistrationStatus::Accepted || preBootTransactions)) {

        #if MO_ENABLE_V16
        if (ocppVersion == MO_OCPP_V16) {
            context.getModel16().activateTasks();
        }
        #endif //MO_ENABLE_V16
        #if MO_ENABLE_V201
        if (ocppVersion == MO_OCPP_V201) {
            context.getModel201().activateTasks();
        }
        #endif //MO_ENABLE_V201

        activatedModel = true;
    }

    if (status == RegistrationStatus::Accepted) {
        return;
    }

    int32_t dtLastBootNotification;
    if (!clock.delta(clock.getUptime(), lastBootNotification, dtLastBootNotification)) {
        dtLastBootNotification = 0;
    }
    
    if (lastBootNotification.isDefined() && dtLastBootNotification < interval_s) {
        return;
    }

    /*
     * Create BootNotification. The BootNotifaction object will fetch its paremeters from
     * this class and notify this class about the response
     */
    auto bootNotification = makeRequest(context, new BootNotification(context, *this, heartbeatService, getBootNotificationData()));
    bootNotification->setTimeout(interval_s);
    context.getMessageService().sendRequestPreBoot(std::move(bootNotification));

    lastBootNotification = clock.getUptime();
}

namespace MicroOcpp {
size_t measureDataEntry(const char *bnDataEntry) {
    return bnDataEntry ? strlen(bnDataEntry) + 1 : 0;
}

bool setDataEntry(char *bnDataBuf, size_t bufsize, size_t& written, const char **bnDataEntryDest, const char *bnDataEntrySrc) {
    if (!bnDataEntrySrc) {
        bnDataEntryDest = nullptr;
        return true;
    }
    *bnDataEntryDest = bnDataBuf + written;
    int ret = snprintf(bnDataBuf + written, bufsize - written, "%s", bnDataEntrySrc);
    if (ret < 0 || (size_t)ret >= bufsize - written) {
        return false;
    }
    written += (size_t)ret + 1;
    return true;
}
}

bool BootService::setBootNotificationData(MO_BootNotificationData bnData) {
    size_t bufsize = 0;
    bufsize += measureDataEntry(bnData.chargePointModel);
    bufsize += measureDataEntry(bnData.chargePointVendor);
    bufsize += measureDataEntry(bnData.firmwareVersion);
    bufsize += measureDataEntry(bnData.chargePointSerialNumber);
    bufsize += measureDataEntry(bnData.meterSerialNumber);
    bufsize += measureDataEntry(bnData.meterType);
    bufsize += measureDataEntry(bnData.chargeBoxSerialNumber);
    bufsize += measureDataEntry(bnData.iccid);
    bufsize += measureDataEntry(bnData.imsi);

    bnDataBuf = static_cast<char*>(MO_MALLOC(getMemoryTag(), bufsize));
    if (!bnDataBuf) {
        MO_DBG_ERR("OOM");
        return false;
    }

    memset(&this->bnData, 0, sizeof(this->bnData));

    size_t written = 0;
    bool success = true;
    if (success) {success &= setDataEntry(bnDataBuf, bufsize, written, &this->bnData.chargePointModel, bnData.chargePointModel);}
    if (success) {success &= setDataEntry(bnDataBuf, bufsize, written, &this->bnData.chargePointVendor, bnData.chargePointVendor);}
    if (success) {success &= setDataEntry(bnDataBuf, bufsize, written, &this->bnData.firmwareVersion, bnData.firmwareVersion);}
    if (success) {success &= setDataEntry(bnDataBuf, bufsize, written, &this->bnData.chargePointSerialNumber, bnData.chargePointSerialNumber);}
    if (success) {success &= setDataEntry(bnDataBuf, bufsize, written, &this->bnData.meterSerialNumber, bnData.meterSerialNumber);}
    if (success) {success &= setDataEntry(bnDataBuf, bufsize, written, &this->bnData.meterType, bnData.meterType);}
    if (success) {success &= setDataEntry(bnDataBuf, bufsize, written, &this->bnData.chargeBoxSerialNumber, bnData.chargeBoxSerialNumber);}
    if (success) {success &= setDataEntry(bnDataBuf, bufsize, written, &this->bnData.iccid, bnData.iccid);}
    if (success) {success &= setDataEntry(bnDataBuf, bufsize, written, &this->bnData.imsi, bnData.imsi);}

    if (!success) {
        MO_DBG_ERR("failure to set BootNotification data");
        goto fail;
    }

    if (written != bufsize) {
        MO_DBG_ERR("internal error");
        goto fail;
    }

    return true;
fail:
    MO_FREE(bnDataBuf);
    memset(&this->bnData, 0, sizeof(this->bnData));
    return false;
}

const MO_BootNotificationData& BootService::getBootNotificationData() {
    return bnData;
}

void BootService::notifyRegistrationStatus(RegistrationStatus status) {
    this->status = status;
    lastBootNotification = context.getClock().getUptime();
}

RegistrationStatus BootService::getRegistrationStatus() {
    return status;
}

void BootService::setRetryInterval(unsigned long interval_s) {
    if (interval_s == 0) {
        this->interval_s = MO_BOOT_INTERVAL_DEFAULT;
    } else {
        this->interval_s = interval_s;
    }
    lastBootNotification = context.getClock().getUptime();
}

#endif //MO_ENABLE_V16 || MO_ENABLE_V201
