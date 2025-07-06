// matth-x/MicroOcpp
// Copyright Matthias Akstaller 2019 - 2024
// MIT License

#include <MicroOcpp/Model/Reset/ResetService.h>

#include <MicroOcpp/Context.h>
#include <MicroOcpp/Model/Model.h>
#include <MicroOcpp/Operations/Reset.h>
#include <MicroOcpp/Operations/StatusNotification.h>
#include <MicroOcpp/Model/Availability/AvailabilityService.h>
#include <MicroOcpp/Model/Configuration/ConfigurationService.h>
#include <MicroOcpp/Model/Variables/VariableService.h>
#include <MicroOcpp/Model/Transactions/TransactionService16.h>
#include <MicroOcpp/Model/Transactions/TransactionService201.h>
#include <MicroOcpp/Debug.h>

#if MO_ENABLE_V16

#ifndef MO_RESET_DELAY
#define MO_RESET_DELAY 10
#endif

#if MO_PLATFORM == MO_PLATFORM_ARDUINO && (defined(ESP32) || defined(ESP8266))

namespace MicroOcpp {
void defaultExecuteResetImpl() {
    MO_DBG_DEBUG("Perform ESP reset");
    ESP.restart();
}
} //namespace MicroOcpp

#else

namespace MicroOcpp {
void (*defaultExecuteResetImpl)() = nullptr;
} //namespace MicroOcpp

#endif //MO_PLATFORM

using namespace MicroOcpp;

v16::ResetService::ResetService(Context& context)
      : MemoryManaged("v16.Reset.ResetService"), context(context) {

}

v16::ResetService::~ResetService() {

}

bool v16::ResetService::setup() {

    auto configService = context.getModel16().getConfigurationService();
    if (!configService) {
        MO_DBG_ERR("setup failure");
        return false;
    }

    resetRetriesInt = configService->declareConfiguration<int>("ResetRetries", 2);
    if (!resetRetriesInt) {
        MO_DBG_ERR("setup failure");
        return false;
    }
    if (!configService->registerValidator<int>("ResetRetries", VALIDATE_UNSIGNED_INT)) {
        MO_DBG_ERR("setup failure");
        return false;
    }

    if (!executeResetCb && defaultExecuteResetImpl) {
        MO_DBG_DEBUG("setup default reset function");
        executeResetCb = [] (bool, void*) {
            defaultExecuteResetImpl();
            return false;
        };
    }

    context.getMessageService().registerOperation("Reset", [] (Context& context) -> Operation* {
        return new Reset(*context.getModel16().getResetService());});

    return true;
}

void v16::ResetService::loop() {

    auto& clock = context.getClock();

    int32_t dtLastResetAttempt;
    if (!clock.delta(clock.getUptime(), lastResetAttempt, dtLastResetAttempt)) {
        dtLastResetAttempt = MO_RESET_DELAY;
    }

    if (outstandingResetRetries > 0 && dtLastResetAttempt >= MO_RESET_DELAY) {
        lastResetAttempt = clock.getUptime();
        outstandingResetRetries--;
        if (executeResetCb) {
            MO_DBG_INFO("Reset device");
            executeResetCb(isHardReset, executeResetUserData);
        } else {
            MO_DBG_ERR("No Reset function set! Abort");
            outstandingResetRetries = 0;
        }

        if (outstandingResetRetries <= 0) {

            MO_DBG_ERR("Reset device failure. Abort");

            auto availSvc = context.getModel16().getAvailabilityService();
            auto availSvcCp = availSvc ? availSvc->getEvse(0) : nullptr;
            auto cpStatus = availSvcCp ? availSvcCp->getStatus() : MO_ChargePointStatus_UNDEFINED;

            MO_ErrorData errorCode;
            mo_ErrorData_init(&errorCode);
            mo_ErrorData_setErrorCode(&errorCode, "ResetFailure");

            auto statusNotification = makeRequest(context, new StatusNotification(
                        context,
                        0,
                        cpStatus,
                        context.getClock().now(),
                        errorCode));
            statusNotification->setTimeout(60);
            context.getMessageService().sendRequest(std::move(statusNotification));
        }
    }
}

void v16::ResetService::setNotifyReset(bool (*notifyResetCb)(bool isHard, void *userData), void *userData) {
    this->notifyResetCb = notifyResetCb;
    this->notifyResetUserData = userData;
}

bool v16::ResetService::isPreResetDefined() {
    return notifyResetCb != nullptr;
}

bool v16::ResetService::notifyReset(bool isHard) {
    return notifyResetCb(isHard, notifyResetUserData);
}

void v16::ResetService::setExecuteReset(bool (*executeReset)(bool isHard, void *userData), void *userData) {
    this->executeResetCb = executeReset;
    this->executeResetUserData = userData;
}

bool v16::ResetService::isExecuteResetDefined() {
    return executeResetCb != nullptr;
}

void v16::ResetService::initiateReset(bool isHard) {

    for (unsigned int eId = 0; eId < context.getModel16().getNumEvseId(); eId++) {
        auto txSvc = context.getModel16().getTransactionService();
        auto txSvcEvse = txSvc ? txSvc->getEvse(eId) : nullptr;
        if (txSvcEvse) {
            txSvcEvse->endTransaction(nullptr, isHard ? "HardReset" : "SoftReset");
        }
    }

    isHardReset = isHard;
    outstandingResetRetries = 1 + resetRetriesInt->getInt(); //one initial try + no. of retries
    if (outstandingResetRetries > 5) {
        MO_DBG_ERR("no. of reset trials exceeds 5");
        outstandingResetRetries = 5;
    }
    lastResetAttempt = context.getClock().getUptime();
}

#endif //MO_ENABLE_V16

#if MO_ENABLE_V201
namespace MicroOcpp {

v201::ResetService::ResetService(Context& context)
      : MemoryManaged("v201.Reset.ResetService"), context(context) {

}

v201::ResetService::~ResetService() {
    for (unsigned int i = 0; i < numEvseId; i++) {
        delete evses[i];
        evses[i] = 0;
    }
}

bool v201::ResetService::setup() {
    auto varService = context.getModel201().getVariableService();
    if (!varService) {
        return false;
    }
    resetRetriesInt = varService->declareVariable<int>("OCPPCommCtrlr", "ResetRetries", 0);
    if (!resetRetriesInt) {
        MO_DBG_ERR("setup failure");
        return false;
    }

    if (defaultExecuteResetImpl) {
        if (!getEvse(0)) {
            MO_DBG_ERR("setup failure");
        }

        if (!getEvse(0)->executeReset) {
            MO_DBG_DEBUG("setup default reset function");
            getEvse(0)->executeReset = [] (unsigned int, void*) -> bool {
                defaultExecuteResetImpl();
                return true;
            };
        }
    }

    numEvseId = context.getModel201().getNumEvseId();
    for (unsigned int i = 0; i < numEvseId; i++) {
        if (!getEvse(i) || !getEvse(i)->setup()) {
            MO_DBG_ERR("setup failure");
            return false;
        }
    }

    context.getMessageService().registerOperation("Reset", [] (Context& context) -> Operation* {
        return new Reset(*context.getModel201().getResetService());});

    return true;
}

v201::ResetService::Evse::Evse(Context& context, ResetService& resetService, unsigned int evseId) : MemoryManaged("v201.Reset.ResetService"), context(context), resetService(resetService), evseId(evseId) {

}

bool v201::ResetService::Evse::setup() {
    auto varService = context.getModel201().getVariableService();
    if (!varService) {
        return false;
    }
    varService->declareVariable<bool>(ComponentId("EVSE", evseId >= 1 ? evseId : -1), "AllowReset", true, Mutability::ReadOnly, false);
    return true;
}

void v201::ResetService::Evse::loop() {

    auto& clock = context.getClock();

    if (outstandingResetRetries && awaitTxStop) {

        for (unsigned int eId = std::max(1U, evseId); eId < (evseId == 0 ? MO_NUM_EVSEID : evseId + 1); eId++) {
            //If evseId > 0, execute this block one time for evseId. If evseId == 0, then iterate over all evseIds > 0

            auto txService = context.getModel201().getTransactionService();
            if (txService && txService->getEvse(eId) && txService->getEvse(eId)->getTransaction()) {
                auto tx = txService->getEvse(eId)->getTransaction();

                if (!tx->stopped) {
                    // wait until tx stopped
                    return;
                }
            }
        }

        awaitTxStop = false;

        MO_DBG_INFO("Reset - tx stopped");
        lastResetAttempt = clock.getUptime(); // wait for some more time until final reset
    }

    int32_t dtLastResetAttempt;
    if (!clock.delta(clock.getUptime(), lastResetAttempt, dtLastResetAttempt)) {
        dtLastResetAttempt = MO_RESET_DELAY;
    }

    if (outstandingResetRetries && dtLastResetAttempt >= MO_RESET_DELAY) {
        lastResetAttempt = clock.getUptime();
        outstandingResetRetries--;

        MO_DBG_INFO("Reset device");

        bool success = executeReset(evseId, executeResetUserData);

        if (success) {
            outstandingResetRetries = 0;

            if (evseId != 0) {
                //Set this EVSE Available again

                auto availSvc = context.getModel201().getAvailabilityService();
                auto availSvcEvse = availSvc ? availSvc->getEvse(evseId) : nullptr;
                if (availSvcEvse) {
                    availSvcEvse->setAvailable(this);
                }
            }
        } else if (!outstandingResetRetries) {
            MO_DBG_ERR("Reset device failure");

            if (evseId == 0) {
                //Set all EVSEs Available again
                for (unsigned int eId = 0; eId < resetService.numEvseId; eId++) {
                    auto availSvc = context.getModel201().getAvailabilityService();
                    auto availSvcEvse = availSvc ? availSvc->getEvse(eId) : nullptr;
                    if (availSvcEvse) {
                        availSvcEvse->setAvailable(this);
                    }
                }
            } else {
                //Set only this EVSE Available
                auto availSvc = context.getModel201().getAvailabilityService();
                auto availSvcEvse = availSvc ? availSvc->getEvse(evseId) : nullptr;
                if (availSvcEvse) {
                    availSvcEvse->setAvailable(this);
                }
            }
        }
    }
}

v201::ResetService::Evse *v201::ResetService::getEvse(unsigned int evseId) {
    if (evseId >= numEvseId) {
        MO_DBG_ERR("evseId out of bound");
        return nullptr;
    }

    if (!evses[evseId]) {
        evses[evseId] = new Evse(context, *this, evseId);
        if (!evses[evseId]) {
            MO_DBG_ERR("OOM");
            return nullptr;
        }
    }

    return evses[evseId];
}

void v201::ResetService::loop() {
    for (unsigned i = 0; i < numEvseId; i++) {
        if (evses[i]) {
            evses[i]->loop();
        }
    }
}

void v201::ResetService::setNotifyReset(unsigned int evseId, bool (*notifyReset)(MO_ResetType, unsigned int evseId, void *userData), void *userData) {
    Evse *evse = getEvse(evseId);
    if (!evse) {
        MO_DBG_ERR("evseId not found");
        return;
    }
    evse->notifyReset = notifyReset;
    evse->notifyResetUserData = userData;
}

void v201::ResetService::setExecuteReset(unsigned int evseId, bool (*executeReset)(unsigned int evseId, void *userData), void *userData) {
    Evse *evse = getEvse(evseId);
    if (!evse) {
        MO_DBG_ERR("evseId not found");
        return;
    }
    evse->executeReset = executeReset;
    evse->executeResetUserData = userData;
}

ResetStatus v201::ResetService::initiateReset(MO_ResetType type, unsigned int evseId) {
    auto evse = getEvse(evseId);
    if (!evse) {
        MO_DBG_ERR("evseId not found");
        return ResetStatus_Rejected;
    }

    if (!evse->executeReset) {
        MO_DBG_INFO("EVSE %u does not support Reset", evseId);
        return ResetStatus_Rejected;
    }

    //Check if EVSEs are ready for Reset
    for (unsigned int eId = evseId; eId < (evseId == 0 ? numEvseId : evseId + 1); eId++) {
        //If evseId > 0, execute this block one time for evseId. If evseId == 0, then iterate over all evseIds

        if (auto it = getEvse(eId)) {
            if (it->notifyReset && !it->notifyReset(type, eId, it->notifyResetUserData)) {
                MO_DBG_INFO("EVSE %u not able to Reset", evseId);
                return ResetStatus_Rejected;
            }
        }
    }

    //Set EVSEs Unavailable
    if (evseId == 0) {
        //Set all EVSEs Unavailable
        for (unsigned int eId = 0; eId < numEvseId; eId++) {
            auto availSvc = context.getModel201().getAvailabilityService();
            auto availSvcEvse = availSvc ? availSvc->getEvse(eId) : nullptr;
            if (availSvcEvse) {
                availSvcEvse->setUnavailable(this);
            }
        }
    } else {
        //Set this EVSE Unavailable
        auto availSvc = context.getModel201().getAvailabilityService();
        auto availSvcEvse = availSvc ? availSvc->getEvse(evseId) : nullptr;
        if (availSvcEvse) {
            availSvcEvse->setUnavailable(this);
        }
    }

    bool scheduled = false;

    //Tx-related behavior: if immediate Reset, stop txs; otherwise schedule Reset
    for (unsigned int eId = std::max(1U, evseId); eId < (evseId == 0 ? MO_NUM_EVSEID : evseId + 1); eId++) {
        //If evseId > 0, execute this block one time for evseId. If evseId == 0, then iterate over all evseIds > 0

        auto txService = context.getModel201().getTransactionService();
        if (txService && txService->getEvse(eId) && txService->getEvse(eId)->getTransaction()) {
            auto tx = txService->getEvse(eId)->getTransaction();
            if (tx->active) {
                //Tx in progress. Check behavior
                if (type == MO_ResetType_Immediate) {
                    txService->getEvse(eId)->abortTransaction(MO_TxStoppedReason_ImmediateReset, MO_TxEventTriggerReason_ResetCommand);
                } else {
                    scheduled = true;
                    break;
                }
            }
        }
    }

    //Actually engage Reset

    if (resetRetriesInt->getInt() >= 5) {
        MO_DBG_ERR("no. of reset trials exceeds 5");
        evse->outstandingResetRetries = 5;
    } else {
        evse->outstandingResetRetries = 1 + resetRetriesInt->getInt(); //one initial try + no. of retries
    }
    evse->lastResetAttempt = context.getClock().getUptime();
    evse->awaitTxStop = scheduled;

    return scheduled ? ResetStatus_Scheduled : ResetStatus_Accepted;
}

} //namespace MicroOcpp
#endif //MO_ENABLE_V201
