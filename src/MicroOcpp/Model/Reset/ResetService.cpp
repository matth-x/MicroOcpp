// matth-x/MicroOcpp
// Copyright Matthias Akstaller 2019 - 2024
// MIT License

#include <MicroOcpp/Model/Reset/ResetService.h>
#include <MicroOcpp/Core/Context.h>
#include <MicroOcpp/Model/Model.h>
#include <MicroOcpp/Core/SimpleRequestFactory.h>
#include <MicroOcpp/Core/Configuration.h>
#include <MicroOcpp/Operations/Reset.h>

#include <MicroOcpp/Operations/Authorize.h>
#include <MicroOcpp/Operations/StartTransaction.h>
#include <MicroOcpp/Operations/StatusNotification.h>
#include <MicroOcpp/Operations/StopTransaction.h>

#include <MicroOcpp/Model/Variables/VariableService.h>
#include <MicroOcpp/Model/Transactions/TransactionService.h>

#include <MicroOcpp/Debug.h>

#ifndef MO_RESET_DELAY
#define MO_RESET_DELAY 10000
#endif

using namespace MicroOcpp;

ResetService::ResetService(Context& context)
      : context(context) {

    resetRetriesInt = declareConfiguration<int>("ResetRetries", 2);

    context.getOperationRegistry().registerOperation("Reset", [&context] () {
        return new Ocpp16::Reset(context.getModel());});
}

ResetService::~ResetService() {

}

void ResetService::loop() {

    if (outstandingResetRetries > 0 && mocpp_tick_ms() - t_resetRetry >= MO_RESET_DELAY) {
        t_resetRetry = mocpp_tick_ms();
        outstandingResetRetries--;
        if (executeReset) {
            MO_DBG_INFO("Reset device");
            executeReset(isHardReset);
        } else {
            MO_DBG_ERR("No Reset function set! Abort");
            outstandingResetRetries = 0;
        }
        MO_DBG_ERR("Reset device failure. %s", outstandingResetRetries == 0 ? "Abort" : "Retry");

        if (outstandingResetRetries <= 0) {
            for (unsigned int cId = 0; cId < context.getModel().getNumConnectors(); cId++) {
                auto connector = context.getModel().getConnector(cId);
                connector->setAvailabilityVolatile(true);
            }

            ChargePointStatus cpStatus = ChargePointStatus_UNDEFINED;
            if (context.getModel().getNumConnectors() > 0) {
                cpStatus = context.getModel().getConnector(0)->getStatus();
            }

            auto statusNotification = makeRequest(new Ocpp16::StatusNotification(
                        0,
                        cpStatus, //will be determined in StatusNotification::initiate
                        context.getModel().getClock().now(),
                        "ResetFailure"));
            statusNotification->setTimeout(60000);
            context.initiateRequest(std::move(statusNotification));
        }
    }
}

void ResetService::setPreReset(std::function<bool(bool)> preReset) {
    this->preReset = preReset;
}

std::function<bool(bool)> ResetService::getPreReset() {
    return this->preReset;
}

void ResetService::setExecuteReset(std::function<void(bool)> executeReset) {
    this->executeReset = executeReset;
}

std::function<void(bool)> ResetService::getExecuteReset() {
    return this->executeReset;
}

void ResetService::initiateReset(bool isHard) {
    isHardReset = isHard;
    outstandingResetRetries = 1 + resetRetriesInt->getInt(); //one initial try + no. of retries
    if (outstandingResetRetries > 5) {
        MO_DBG_ERR("no. of reset trials exceeds 5");
        outstandingResetRetries = 5;
    }
    t_resetRetry = mocpp_tick_ms();

    for (unsigned int cId = 0; cId < context.getModel().getNumConnectors(); cId++) {
        auto connector = context.getModel().getConnector(cId);
        connector->setAvailabilityVolatile(false);
    }
}

#if MO_PLATFORM == MO_PLATFORM_ARDUINO && (defined(ESP32) || defined(ESP8266))
std::function<void(bool isHard)> MicroOcpp::makeDefaultResetFn() {
    return [] (bool isHard) {
        MO_DBG_DEBUG("Perform ESP reset");
        ESP.restart();
    };
}
#endif //MO_PLATFORM == MO_PLATFORM_ARDUINO && (defined(ESP32) || defined(ESP8266))

#if MO_ENABLE_V201
namespace MicroOcpp {
namespace Ocpp201 {

ResetService::ResetService(Context& context)
      : context(context) {

    auto varService = context.getModel().getVariableService();
    resetRetriesInt = varService->declareVariable<int>("OCPPCommCtrlr", "ResetRetries", 0);

    context.getOperationRegistry().registerOperation("Reset", [this] () {
        return new Ocpp201::Reset(*this);});
}

ResetService::~ResetService() {

}

ResetService::Evse::Evse(Context& context, ResetService& resetService, unsigned int evseId) : context(context), resetService(resetService), evseId(evseId) {
    auto varService = context.getModel().getVariableService();
    varService->declareVariable<bool>(ComponentId("EVSE", evseId), "AllowReset", true, MO_VARIABLE_FN, Variable::Mutability::ReadOnly);
}

void ResetService::Evse::loop() {

    if (outstandingResetRetries && awaitTxStop) {

        for (unsigned int eId = std::max(1U, evseId); eId < (evseId == 0 ? MO_NUM_EVSE : evseId + 1); eId++) {
            //If evseId > 0, execute this block one time for evseId. If evseId == 0, then iterate over all evseIds > 0

            auto txService = context.getModel().getTransactionService();
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
        t_resetRetry = mocpp_tick_ms(); // wait for some more time until final reset
    }

    if (outstandingResetRetries && mocpp_tick_ms() - t_resetRetry >= MO_RESET_DELAY) {
        t_resetRetry = mocpp_tick_ms();
        outstandingResetRetries--;

        MO_DBG_INFO("Reset device");

        bool success = executeReset();

        if (success) {
            outstandingResetRetries = 0;

            if (evseId != 0) {
                //Set this EVSE Available again
                if (auto connector = context.getModel().getConnector(evseId)) {
                    connector->setAvailabilityVolatile(true);
                }
            }
        } else if (!outstandingResetRetries) {
            MO_DBG_ERR("Reset device failure");

            if (evseId == 0) {
                //Set all EVSEs Available again
                for (unsigned int cId = 0; cId < context.getModel().getNumConnectors(); cId++) {
                    auto connector = context.getModel().getConnector(cId);
                    connector->setAvailabilityVolatile(true);
                }
            } else {
                //Set only this EVSE Available
                if (auto connector = context.getModel().getConnector(evseId)) {
                    connector->setAvailabilityVolatile(true);
                }
            }
        }
    }
}

ResetService::Evse *ResetService::getEvse(unsigned int evseId) {
    for (size_t i = 0; i < evses.size(); i++) {
        if (evses[i].evseId == evseId) {
            return &evses[i];
        }
    }
    return nullptr;
}

ResetService::Evse *ResetService::getOrCreateEvse(unsigned int evseId) {
    if (auto evse = getEvse(evseId)) {
        return evse;
    }

    if (evseId >= MO_NUM_EVSE) {
        MO_DBG_ERR("evseId out of bound");
        return nullptr;
    }

    evses.emplace_back(context, *this, evseId);
    return &evses.back();
}

void ResetService::loop() {
    for (Evse& evse : evses) {
        evse.loop();
    }
}

void ResetService::setNotifyReset(std::function<bool(ResetType)> notifyReset, unsigned int evseId) {
    Evse *evse = getOrCreateEvse(evseId);
    if (!evse) {
        MO_DBG_ERR("evseId not found");
        return;
    }
    evse->notifyReset = notifyReset;
}

std::function<bool(ResetType)> ResetService::getNotifyReset(unsigned int evseId) {
    Evse *evse = getOrCreateEvse(evseId);
    if (!evse) {
        MO_DBG_ERR("evseId not found");
        return nullptr;
    }
    return evse->notifyReset;
}

void ResetService::setExecuteReset(std::function<bool()> executeReset, unsigned int evseId) {
    Evse *evse = getOrCreateEvse(evseId);
    if (!evse) {
        MO_DBG_ERR("evseId not found");
        return;
    }
    evse->executeReset = executeReset;
}

std::function<bool()> ResetService::getExecuteReset(unsigned int evseId) {
    Evse *evse = getOrCreateEvse(evseId);
    if (!evse) {
        MO_DBG_ERR("evseId not found");
        return nullptr;
    }
    return evse->executeReset;
}

ResetStatus ResetService::initiateReset(ResetType type, unsigned int evseId) {
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
    for (unsigned int eId = evseId; eId < (evseId == 0 ? MO_NUM_EVSE : evseId + 1); eId++) {
        //If evseId > 0, execute this block one time for evseId. If evseId == 0, then iterate over all evseIds

        if (auto it = getEvse(eId)) {
            if (it->notifyReset && !it->notifyReset(type)) {
                MO_DBG_INFO("EVSE %u not able to Reset", evseId);
                return ResetStatus_Rejected;
            }
        }
    }

    //Set EVSEs Unavailable
    if (evseId == 0) {
        //Set all EVSEs Unavailable
        for (unsigned int cId = 0; cId < context.getModel().getNumConnectors(); cId++) {
            auto connector = context.getModel().getConnector(cId);
            connector->setAvailabilityVolatile(false);
        }
    } else {
        //Set this EVSE Unavailable
        if (auto connector = context.getModel().getConnector(evseId)) {
            connector->setAvailabilityVolatile(false);
        }
    }

    bool scheduled = false;

    //Tx-related behavior: if immediate Reset, stop txs; otherwise schedule Reset
    for (unsigned int eId = std::max(1U, evseId); eId < (evseId == 0 ? MO_NUM_EVSE : evseId + 1); eId++) {
        //If evseId > 0, execute this block one time for evseId. If evseId == 0, then iterate over all evseIds > 0

        auto txService = context.getModel().getTransactionService();
        if (txService && txService->getEvse(eId) && txService->getEvse(eId)->getTransaction()) {
            auto tx = txService->getEvse(eId)->getTransaction();
            if (tx->active) {
                //Tx in progress. Check behavior
                if (type == ResetType_Immediate) {
                    txService->getEvse(eId)->abortTransaction(Transaction::StopReason::ImmediateReset, TransactionEventTriggerReason::ResetCommand);
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
    evse->t_resetRetry = mocpp_tick_ms();
    evse->awaitTxStop = scheduled;

    return scheduled ? ResetStatus_Scheduled : ResetStatus_Accepted;
}

} //namespace MicroOcpp
} //namespace Ocpp201
#endif //MO_ENABLE_V201
