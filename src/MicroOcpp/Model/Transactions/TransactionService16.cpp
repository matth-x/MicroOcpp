// matth-x/MicroOcpp
// Copyright Matthias Akstaller 2019 - 2024
// MIT License

#include <MicroOcpp/Model/Transactions/TransactionService16.h>

#include <MicroOcpp/Context.h>
#include <MicroOcpp/Model/Model.h>
#include <MicroOcpp/Core/Request.h>
#include <MicroOcpp/Model/Transactions/TransactionStore.h>
#include <MicroOcpp/Model/Configuration/ConfigurationService.h>

#include <MicroOcpp/Operations/Authorize.h>
#include <MicroOcpp/Operations/ClearCache.h>
#include <MicroOcpp/Operations/StatusNotification.h>
#include <MicroOcpp/Operations/StartTransaction.h>
#include <MicroOcpp/Operations/StopTransaction.h>

#include <MicroOcpp/Debug.h>

#include <MicroOcpp/Model/Availability/AvailabilityService.h>
#include <MicroOcpp/Model/Metering/MeteringService.h>
#include <MicroOcpp/Model/Metering/MeterStore.h>
#include <MicroOcpp/Model/Reservation/ReservationService.h>
#include <MicroOcpp/Model/Authorization/AuthorizationService.h>
#include <MicroOcpp/Model/Common/EvseId.h>

#include <MicroOcpp/Core/Request.h>
#include <MicroOcpp/Core/Connection.h>

#ifndef MO_TX_CLEAN_ABORTED
#define MO_TX_CLEAN_ABORTED 1
#endif

using namespace MicroOcpp;
using namespace MicroOcpp::v16;

TransactionServiceEvse::TransactionServiceEvse(Context& context, TransactionService& cService, unsigned int evseId)
        : MemoryManaged("v16.Transactions.TransactionServiceEvse"), context(context), clock(context.getClock()), model(context.getModel16()), cService(cService), evseId(evseId) {

}

TransactionServiceEvse::~TransactionServiceEvse() {
    if (transaction != transactionFront) {
        delete transaction;
    }
    transaction = nullptr;
    delete transactionFront;
    transactionFront = nullptr;
}

bool TransactionServiceEvse::setup() {

    context.getMessageService().addSendQueue(this); //register at RequestQueue as Request emitter

    connection = context.getConnection();
    if (!connection) {
        MO_DBG_ERR("setup failure");
        return false;
    }

    meteringServiceEvse = model.getMeteringService() ? model.getMeteringService()->getEvse(evseId) : nullptr;
    if (!meteringServiceEvse) {
        MO_DBG_ERR("setup failure");
        return false;
    }

    availServiceEvse = model.getAvailabilityService() ? model.getAvailabilityService()->getEvse(evseId) : nullptr;
    if (!availServiceEvse) {
        MO_DBG_ERR("setup failure");
        return false;
    }

    filesystem = context.getFilesystem();
    if (!filesystem) {
        MO_DBG_DEBUG("no FS access. Can enqueue only one transaction");
    }

    char txFnamePrefix [30];
    snprintf(txFnamePrefix, sizeof(txFnamePrefix), "tx-%.*u-", MO_NUM_EVSEID_DIGITS, evseId);

    if (filesystem) {
        if (!FilesystemUtils::loadRingIndex(filesystem, txFnamePrefix, MO_TXNR_MAX, &txNrBegin, &txNrEnd)) {
            MO_DBG_ERR("failed to load tx index");
            return false;
        }
    }

    MO_DBG_DEBUG("found %u transactions for connector %u. Internal range from %u to %u (exclusive)", (txNrEnd + MO_TXNR_MAX - txNrBegin) % MO_TXNR_MAX, evseId, txNrBegin, txNrEnd);
    txNrFront = txNrBegin;

    if (filesystem) {
        unsigned int txNrLatest = (txNrEnd + MO_TXNR_MAX - 1) % MO_TXNR_MAX; //txNr of the most recent tx on flash
        auto txLoaded = new Transaction(evseId, txNrLatest);
        if (!txLoaded) {
            MO_DBG_ERR("OOM");
            return false;
        }

        bool setupFailure = false;

        auto ret = TransactionStore::load(filesystem, context, evseId, txNrLatest, *txLoaded);
        switch (ret) {
            case FilesystemUtils::LoadStatus::Success:
                //continue loading txMeterValues
                break;
            case FilesystemUtils::LoadStatus::FileNotFound:
                break;
            case FilesystemUtils::LoadStatus::ErrOOM:
                MO_DBG_ERR("OOM");
                setupFailure = true;
                break;
            case FilesystemUtils::LoadStatus::ErrFileCorruption:
            case FilesystemUtils::LoadStatus::ErrOther:
                MO_DBG_ERR("tx load failure"); //leave the corrupt file on flash and wait for the queue front to reach and clean it
                break;
        }

        if (ret == FilesystemUtils::LoadStatus::Success) {
            auto ret2 = MeterStore::load(filesystem, context, meteringServiceEvse->getMeterInputs(), evseId, txNrLatest, txLoaded->getTxMeterValues());
            switch (ret2) {
                case FilesystemUtils::LoadStatus::Success:
                case FilesystemUtils::LoadStatus::FileNotFound:
                    transaction = txLoaded;
                    txLoaded = nullptr;
                    break;
                case FilesystemUtils::LoadStatus::ErrOOM:
                    MO_DBG_ERR("OOM");
                    setupFailure = true;
                    break;
                case FilesystemUtils::LoadStatus::ErrFileCorruption:
                case FilesystemUtils::LoadStatus::ErrOther:
                    MO_DBG_ERR("tx load failure");
                    MeterStore::remove(filesystem, evseId, txNrLatest);
                    transaction = txLoaded;
                    txLoaded = nullptr;
                    break;
            }
        }

        delete txLoaded;
        txLoaded = nullptr;

        if (setupFailure) {
            return false;
        }
    }

    return true;
}

bool TransactionServiceEvse::ocppPermitsCharge() {
    if (evseId == 0) {
        MO_DBG_WARN("not supported for evseId == 0");
        return false;
    }

    bool suspendDeAuthorizedIdTag = transaction && transaction->isIdTagDeauthorized(); //if idTag status is "DeAuthorized" and if charging should stop

    //check special case for DeAuthorized idTags: FreeVend mode
    if (suspendDeAuthorizedIdTag && cService.freeVendActiveBool->getBool()) {
        suspendDeAuthorizedIdTag = false;
    }

    // check charge permission depending on TxStartPoint
    if (cService.txStartOnPowerPathClosedBool->getBool()) {
        // tx starts when the power path is closed. Advertise charging before transaction
        return transaction &&
                transaction->isActive() &&
                transaction->isAuthorized() &&
                !suspendDeAuthorizedIdTag;
    } else {
        // tx must be started before the power path can be closed
        return transaction &&
               transaction->isRunning() &&
               transaction->isActive() &&
               !suspendDeAuthorizedIdTag;
    }
}

void TransactionServiceEvse::loop() {

    if (!trackLoopExecute) {
        trackLoopExecute = true;
        if (connectorPluggedInput) {
            freeVendTrackPlugged = connectorPluggedInput(evseId, connectorPluggedInputUserData);
        }
    }

    if (transaction && filesystem && ((transaction->isAborted() && MO_TX_CLEAN_ABORTED) || (transaction->isSilent() && transaction->getStopSync().isRequested()))) {
        //If the transaction is aborted (invalidated before started) or is silent and has stopped. Delete all artifacts from flash
        //This is an optimization. The memory management will attempt to remove those files again later
        bool removed = MeterStore::remove(filesystem, evseId, transaction->getTxNr());

        if (removed) {
            removed &= TransactionStore::remove(filesystem, evseId, transaction->getTxNr());
        }

        if (removed) {
            if (txNrFront == txNrEnd) {
                txNrFront = transaction->getTxNr();
            }
            txNrEnd = transaction->getTxNr(); //roll back creation of last tx entry
        }

        MO_DBG_DEBUG("collect aborted or silent transaction %u-%u %s", evseId, transaction->getTxNr(), removed ? "" : "failure");
        MO_DBG_VERBOSE("txNrBegin=%u, txNrFront=%u, txNrEnd=%u", txNrBegin, txNrFront, txNrEnd);
        if (transaction != transactionFront) {
            delete transaction;
        }
        transaction = nullptr;
    }

    if (transaction && transaction->isAborted()) {
        MO_DBG_DEBUG("collect aborted transaction %u-%u", evseId, transaction->getTxNr());
        MO_DBG_VERBOSE("txNrBegin=%u, txNrFront=%u, txNrEnd=%u", txNrBegin, txNrFront, txNrEnd);
        if (transaction != transactionFront) {
            delete transaction;
        }
        transaction = nullptr;
    }

    if (transaction && transaction->getStopSync().isRequested()) {
        MO_DBG_DEBUG("collect obsolete transaction %u-%u", evseId, transaction->getTxNr());
        MO_DBG_VERBOSE("txNrBegin=%u, txNrFront=%u, txNrEnd=%u", txNrBegin, txNrFront, txNrEnd);
        if (transaction != transactionFront) {
            delete transaction;
        }
        transaction = nullptr;
    }

    if (transaction) { //begin exclusively transaction-related operations

        if (connectorPluggedInput) {
            if (transaction->isRunning() && transaction->isActive() && !connectorPluggedInput(evseId, connectorPluggedInputUserData)) {
                if (cService.stopTransactionOnEVSideDisconnectBool->getBool()) {
                    MO_DBG_DEBUG("Stop Tx due to EV disconnect");
                    transaction->setStopReason("EVDisconnected");
                    transaction->setInactive();
                    commitTransaction(transaction);
                }
            }

            int32_t runtimeSecs;
            if (!clock.delta(clock.now(), transaction->getBeginTimestamp(), runtimeSecs)) {
                runtimeSecs = 0;
            }

            if (transaction->isActive() &&
                    !transaction->getStartSync().isRequested() &&
                    cService.connectionTimeOutInt->getInt() > 0 &&
                    !connectorPluggedInput(evseId, connectorPluggedInputUserData) &&
                    runtimeSecs >= cService.connectionTimeOutInt->getInt()) {

                MO_DBG_INFO("Session mngt: timeout");
                transaction->setInactive();
                commitTransaction(transaction);

                updateTxNotification(MO_TxNotification_ConnectionTimeout);
            }
        }

        if (transaction->isActive() &&
                transaction->isIdTagDeauthorized() && ( //transaction has been deAuthorized
                    !transaction->isRunning() ||        //if transaction hasn't started yet, always end
                    cService.stopTransactionOnInvalidIdBool->getBool())) { //if transaction is running, behavior depends on StopTransactionOnInvalidId

            MO_DBG_DEBUG("DeAuthorize session");
            transaction->setStopReason("DeAuthorized");
            transaction->setInactive();
            commitTransaction(transaction);
        }

        /*
        * Check conditions for start or stop transaction
        */

        if (!transaction->isRunning()) {
            //start tx?

            if (transaction->isActive() && transaction->isAuthorized() &&  //tx must be authorized
                    (!connectorPluggedInput || connectorPluggedInput(evseId, connectorPluggedInputUserData)) && //if applicable, connector must be plugged
                    availServiceEvse->isOperative() && //only start tx if charger is free of error conditions
                    (!cService.txStartOnPowerPathClosedBool->getBool() || !evReadyInput || evReadyInput(evseId, evReadyInputUserData)) && //if applicable, postpone tx start point to PowerPathClosed
                    (!startTxReadyInput || startTxReadyInput(evseId, startTxReadyInputUserData))) { //if defined, user Input for allowing StartTx must be true
                //start Transaction

                MO_DBG_INFO("Session mngt: trigger StartTransaction");

                if (transaction->getMeterStart() < 0) {
                    auto meterStart = meteringServiceEvse->readTxEnergyMeter(MO_ReadingContext_TransactionBegin);
                    transaction->setMeterStart(meterStart);
                }

                if (!transaction->getStartTimestamp().isDefined()) {
                    transaction->setStartTimestamp(clock.now());
                }

                transaction->getStartSync().setRequested();
                transaction->getStartSync().setOpNr(context.getMessageService().getNextOpNr());

                if (transaction->isSilent()) {
                    MO_DBG_INFO("silent Transaction: omit StartTx");
                    transaction->getStartSync().confirm();
                } else {
                    //normal transaction, record txMeterData
                    meteringServiceEvse->beginTxMeterData(transaction);
                }

                commitTransaction(transaction);

                updateTxNotification(MO_TxNotification_StartTx);

                //fetchFrontRequest will create the StartTransaction and pass it to the message sender
                return;
            }
        } else {
            //stop tx?

            if (!transaction->isActive() &&
                    (!stopTxReadyInput || stopTxReadyInput(evseId, stopTxReadyInputUserData))) {
                //stop transaction

                MO_DBG_INFO("Session mngt: trigger StopTransaction");

                if (transaction->getMeterStop() < 0) {
                    auto meterStop = meteringServiceEvse->readTxEnergyMeter(MO_ReadingContext_TransactionEnd);
                    transaction->setMeterStop(meterStop);
                }

                if (!transaction->getStopTimestamp().isDefined()) {
                    transaction->setStopTimestamp(clock.now());
                }

                transaction->getStopSync().setRequested();
                transaction->getStopSync().setOpNr(context.getMessageService().getNextOpNr());

                if (transaction->isSilent()) {
                    MO_DBG_INFO("silent Transaction: omit StopTx");
                    transaction->getStopSync().confirm();
                } else {
                    //normal transaction, record txMeterData
                    meteringServiceEvse->endTxMeterData(transaction);
                }

                commitTransaction(transaction);

                updateTxNotification(MO_TxNotification_StopTx);

                //fetchFrontRequest will create the StopTransaction and pass it to the message sender
                return;
            }
        }
    } //end transaction-related operations

    //handle FreeVend mode
    if (cService.freeVendActiveBool->getBool() && connectorPluggedInput) {
        if (!freeVendTrackPlugged && connectorPluggedInput(evseId, connectorPluggedInputUserData) && !transaction) {
            const char *idTag = cService.freeVendIdTagString->getString();
            MO_DBG_INFO("begin FreeVend Tx using idTag %s", idTag);
            beginTransaction_authorized(idTag);

            if (!transaction) {
                MO_DBG_ERR("could not begin FreeVend Tx");
            }
        }

        freeVendTrackPlugged = connectorPluggedInput(evseId, connectorPluggedInputUserData);
    }


    return;
}

bool TransactionServiceEvse::commitTransaction(Transaction *transaction) {
    if (filesystem) {
        auto ret = TransactionStore::store(filesystem, context, *transaction);
        if (ret != FilesystemUtils::StoreStatus::Success) {
            MO_DBG_ERR("tx commit failure");
            return false;
        }
    }
    return true;
}

Transaction *TransactionServiceEvse::allocateTransaction() {

    Transaction *tx = nullptr;

    if (!filesystem) {
        //volatile mode shortcuts file restore and capacity logic
        if (transactionFront) {
            MO_DBG_WARN("volatile mode: cannot start tx while older is still in progress");
            return nullptr;
        }
        tx = new Transaction(evseId, txNrEnd);
        if (!tx) {
            MO_DBG_ERR("OOM");
            return nullptr;
        }
        txNrEnd = (txNrEnd + 1) % MO_TXNR_MAX;
        MO_DBG_DEBUG("advance txNrEnd %u-%u", evseId, txNrEnd);
        MO_DBG_VERBOSE("txNrBegin=%u, txNrFront=%u, txNrEnd=%u", txNrBegin, txNrFront, txNrEnd);
        return tx;
    }

    //clean possible aborted tx
    unsigned int txr = txNrEnd;
    unsigned int txSize = (txNrEnd + MO_TXNR_MAX - txNrBegin) % MO_TXNR_MAX;
    for (unsigned int i = 0; i < txSize; i++) {
        txr = (txr + MO_TXNR_MAX - 1) % MO_TXNR_MAX; //decrement by 1

        Transaction txLoaded {evseId, txr};

        auto ret = TransactionStore::load(filesystem, context, evseId, txr, txLoaded);
        if (ret == FilesystemUtils::LoadStatus::ErrOOM) {
            MO_DBG_ERR("OOM");
            goto fail;
        }

        bool found = (ret != FilesystemUtils::LoadStatus::Success); //possible to restore tx from this slot, i.e. exists and non-corrupt

        //check if dangling silent tx, aborted tx, or corrupted entry (tx == null)
        if (!found || txLoaded.isSilent() || (txLoaded.isAborted() && MO_TX_CLEAN_ABORTED)) {
            //yes, remove
            bool removed = MeterStore::remove(filesystem, evseId, txr);
            if (removed) {
                removed &= TransactionStore::remove(filesystem, evseId, txr);
            }
            if (removed) {
                if (txNrFront == txNrEnd) {
                    txNrFront = txr;
                }
                txNrEnd = txr;
                MO_DBG_WARN("deleted dangling silent or aborted tx for new transaction");
            } else {
                MO_DBG_ERR("memory corruption");
                break;
            }
        } else {
            //no, tx record trimmed, end
            break;
        }
    }

    txSize = (txNrEnd + MO_TXNR_MAX - txNrBegin) % MO_TXNR_MAX; //refresh after cleaning txs

    //try to create new transaction
    if (txSize < MO_TXRECORD_SIZE) {
        tx = new Transaction(evseId, txNrEnd);
        if (!tx) {
            MO_DBG_ERR("OOM");
            goto fail;
        }
    }

    if (!tx) {
        //could not create transaction - now, try to replace tx history entry

        unsigned int txl = txNrBegin;
        txSize = (txNrEnd + MO_TXNR_MAX - txNrBegin) % MO_TXNR_MAX;

        for (unsigned int i = 0; i < txSize; i++) {

            if (tx) {
                //success, finished here
                break;
            }

            //no transaction allocated, delete history entry to make space

            Transaction txhist {evseId, txl};

            auto ret = TransactionStore::load(filesystem, context, evseId, txl, txhist);
            if (ret == FilesystemUtils::LoadStatus::ErrOOM) {
                MO_DBG_ERR("OOM");
                goto fail;
            }

            bool found = (ret != FilesystemUtils::LoadStatus::Success); //possible to restore tx from this slot, i.e. exists and non-corrupt

            //oldest entry, now check if it's history and can be removed or corrupted entry
            if (!found || txhist.isCompleted() || txhist.isAborted() || (txhist.isSilent() && txhist.getStopSync().isRequested())) {
                //yes, remove
                bool removed = MeterStore::remove(filesystem, evseId, txl);
                if (removed) {
                    removed &= TransactionStore::remove(filesystem, evseId, txl);
                }
                if (removed) {
                    txNrBegin = (txl + 1) % MO_TXNR_MAX;
                    if (txNrFront == txl) {
                        txNrFront = txNrBegin;
                    }
                    MO_DBG_DEBUG("deleted tx history entry for new transaction");
                    MO_DBG_VERBOSE("txNrBegin=%u, txNrFront=%u, txNrEnd=%u", txNrBegin, txNrFront, txNrEnd);

                    tx = new Transaction(evseId, txNrEnd);
                    if (!tx) {
                        MO_DBG_ERR("OOM");
                        goto fail;
                    }
                } else {
                    MO_DBG_ERR("memory corruption");
                    break;
                }
            } else {
                //no, end of history reached, don't delete further tx
                MO_DBG_DEBUG("cannot delete more tx");
                break;
            }

            txl++;
            txl %= MO_TXNR_MAX;
        }
    }

    if (!tx) {
        //couldn't create normal transaction -> check if to start charging without real transaction
        if (cService.silentOfflineTransactionsBool->getBool()) {
            //run charging session without sending StartTx or StopTx to the server
            tx = new Transaction(evseId, txNrEnd, true);
            if (!tx) {
                MO_DBG_ERR("OOM");
                goto fail;
            }
            MO_DBG_DEBUG("created silent transaction");
        }
    }

    if (tx) {
        //clean meter data which could still be here from a rolled-back transaction
        if (!MeterStore::remove(filesystem, evseId, tx->getTxNr())) {
            MO_DBG_ERR("memory corruption");
        }
    }

    if (tx) {
        txNrEnd = (txNrEnd + 1) % MO_TXNR_MAX;
        MO_DBG_DEBUG("advance txNrEnd %u-%u", evseId, txNrEnd);
        MO_DBG_VERBOSE("txNrBegin=%u, txNrFront=%u, txNrEnd=%u", txNrBegin, txNrFront, txNrEnd);
    }

    return tx;
fail:
    delete tx;
    return nullptr;
}

bool TransactionServiceEvse::beginTransaction(const char *idTag) {

    if (transaction) {
        MO_DBG_WARN("tx process still running. Please call endTransaction(...) before");
        return false;
    }

    MO_DBG_DEBUG("Begin transaction process (%s), prepare", idTag != nullptr ? idTag : "");

    bool localAuthFound = false;
    const char *parentIdTag = nullptr; //locally stored parentIdTag
    bool offlineBlockedAuth = false; //if offline authorization will be blocked by local auth list entry

    //check local OCPP whitelist
    #if MO_ENABLE_LOCAL_AUTH
    if (auto authService = model.getAuthorizationService()) {
        auto localAuth = authService->getLocalAuthorization(idTag);

        //check authorization status
        if (localAuth && localAuth->getAuthorizationStatus() != AuthorizationStatus::Accepted) {
            MO_DBG_DEBUG("local auth denied (%s)", idTag);
            offlineBlockedAuth = true;
            localAuth = nullptr;
        }

        //check expiry
        int32_t dtExpiryDate;
        if (localAuth && localAuth->getExpiryDate() && clock.delta(clock.now(), *localAuth->getExpiryDate(), dtExpiryDate) && dtExpiryDate >= 0) {
            MO_DBG_DEBUG("idTag %s local auth entry expired", idTag);
            offlineBlockedAuth = true;
            localAuth = nullptr;
        }

        if (localAuth) {
            localAuthFound = true;
            parentIdTag = localAuth->getParentIdTag();
        }
    }
    #endif //MO_ENABLE_LOCAL_AUTH

    int reservationId = -1;
    bool offlineBlockedResv = false; //if offline authorization will be blocked by reservation

    //check if blocked by reservation
    #if MO_ENABLE_RESERVATION
    if (model.getReservationService()) {

        auto reservation = model.getReservationService()->getReservation(
                evseId,
                idTag,
                parentIdTag);

        if (reservation) {
            reservationId = reservation->getReservationId();
        }

        if (reservation && !reservation->matches(
                    idTag,
                    parentIdTag)) {
            //reservation blocks connector
            offlineBlockedResv = true; //when offline, tx is always blocked

            //if parentIdTag is known, abort this tx immediately, otherwise wait for Authorize.conf to decide
            if (parentIdTag) {
                //parentIdTag known
                MO_DBG_INFO("connector %u reserved - abort transaction", evseId);
                updateTxNotification(MO_TxNotification_ReservationConflict);
                return false;
            } else {
                //parentIdTag unkown but local authorization failed in any case
                MO_DBG_INFO("connector %u reserved - no local auth", evseId);
                localAuthFound = false;
            }
        }
    }
    #endif //MO_ENABLE_RESERVATION

    transaction = allocateTransaction();

    if (!transaction) {
        MO_DBG_WARN("tx queue full - cannot start another tx");
        return false;
    }

    if (!idTag || *idTag == '\0') {
        //input string is empty
        transaction->setIdTag("");
    } else {
        transaction->setIdTag(idTag);
    }

    if (parentIdTag) {
        transaction->setParentIdTag(parentIdTag);
    }

    transaction->setBeginTimestamp(clock.now());

    //check for local preauthorization
    if (localAuthFound && cService.localPreAuthorizeBool->getBool()) {
        MO_DBG_DEBUG("Begin transaction process (%s), preauthorized locally", idTag != nullptr ? idTag : "");

        if (reservationId >= 0) {
            transaction->setReservationId(reservationId);
        }
        transaction->setAuthorized();

        updateTxNotification(MO_TxNotification_Authorized);
    }

    commitTransaction(transaction);

    auto authorize = makeRequest(context, new Authorize(model, idTag));
    authorize->setTimeout(cService.authorizationTimeoutInt->getInt() > 0 ? cService.authorizationTimeoutInt->getInt(): 20);

    if (!connection->isConnected()) {
        //WebSockt unconnected. Enter offline mode immediately
        authorize->setTimeout(1);
    }

    //Sending Authorize, but operation could have become outdated when the response from the server arrives. Should discard
    //it then. To match the server response with currently active tx object, capture tx identifiers
    unsigned int txNr_capture = transaction->getTxNr();
    Timestamp beginTimestamp_capture = transaction->getBeginTimestamp();

    authorize->setOnReceiveConf([this, txNr_capture, beginTimestamp_capture] (JsonObject response) {

        int32_t dtBeginTimestamp = 0;
        if (!transaction || !context.getClock().delta(transaction->getBeginTimestamp(), beginTimestamp_capture, dtBeginTimestamp)) {
            dtBeginTimestamp = 0;
        }

        if (!transaction || transaction->getTxNr() != txNr_capture || dtBeginTimestamp != 0) {
            MO_DBG_DEBUG("stale Authorize");
            return;
        }

        JsonObject idTagInfo = response["idTagInfo"];

        if (strcmp("Accepted", idTagInfo["status"] | "UNDEFINED")) {
            //Authorization rejected, abort transaction
            MO_DBG_DEBUG("Authorize rejected (%s), abort tx process", transaction->getIdTag());
            transaction->setIdTagDeauthorized();
            commitTransaction(transaction);
            updateTxNotification(MO_TxNotification_AuthorizationRejected);
            return;
        }

        #if MO_ENABLE_RESERVATION
        if (model.getReservationService()) {
            auto reservation = model.getReservationService()->getReservation(
                        evseId,
                        transaction->getIdTag(),
                        idTagInfo["parentIdTag"] | (const char*) nullptr);
            if (reservation) {
                //reservation found for connector
                if (reservation->matches(
                            transaction->getIdTag(),
                            idTagInfo["parentIdTag"] | (const char*) nullptr)) {
                    MO_DBG_INFO("connector %u matches reservationId %i", evseId, reservation->getReservationId());
                    transaction->setReservationId(reservation->getReservationId());
                } else {
                    //reservation found for connector but does not match idTag or parentIdTag
                    MO_DBG_INFO("connector %u reserved - abort transaction", evseId);
                    transaction->setInactive();
                    commitTransaction(transaction);
                    updateTxNotification(MO_TxNotification_ReservationConflict);
                    return;
                }
            }
        }
        #endif //MO_ENABLE_RESERVATION

        if (idTagInfo.containsKey("parentIdTag")) {
            transaction->setParentIdTag(idTagInfo["parentIdTag"] | "");
        }

        MO_DBG_DEBUG("Authorized transaction process (%s)", transaction->getIdTag());
        transaction->setAuthorized();
        commitTransaction(transaction);

        updateTxNotification(MO_TxNotification_Authorized);
    });

    //capture local auth and reservation check in for timeout handler
    authorize->setOnTimeout([this, txNr_capture, beginTimestamp_capture,
                offlineBlockedAuth,
                offlineBlockedResv,
                localAuthFound,
                reservationId] () {

        int32_t dtBeginTimestamp = 0;
        if (!transaction || !context.getClock().delta(transaction->getBeginTimestamp(), beginTimestamp_capture, dtBeginTimestamp)) {
            dtBeginTimestamp = 0;
        }

        if (!transaction || transaction->getTxNr() != txNr_capture || dtBeginTimestamp != 0) {
            MO_DBG_DEBUG("stale Authorize");
            return;
        }

        if (offlineBlockedAuth) {
            //local auth entry exists, but is expired -> avoid offline tx
            MO_DBG_DEBUG("Abort transaction process (%s), timeout, expired local auth", transaction->getIdTag());
            transaction->setInactive();
            commitTransaction(transaction);
            updateTxNotification(MO_TxNotification_AuthorizationTimeout);
            return;
        }

        if (offlineBlockedResv) {
            //reservation found for connector but does not match idTag or parentIdTag
            MO_DBG_INFO("connector %u reserved (offline) - abort transaction", evseId);
            transaction->setInactive();
            commitTransaction(transaction);
            updateTxNotification(MO_TxNotification_ReservationConflict);
            return;
        }

        if (localAuthFound && cService.localAuthorizeOfflineBool->getBool()) {
            MO_DBG_DEBUG("Offline transaction process (%s), locally authorized", transaction->getIdTag());
            if (reservationId >= 0) {
                transaction->setReservationId(reservationId);
            }
            transaction->setAuthorized();
            commitTransaction(transaction);

            updateTxNotification(MO_TxNotification_Authorized);
            return;
        }

        if (cService.allowOfflineTxForUnknownIdBool->getBool()) {
            MO_DBG_DEBUG("Offline transaction process (%s), allow unknown ID", transaction->getIdTag());
            if (reservationId >= 0) {
                transaction->setReservationId(reservationId);
            }
            transaction->setAuthorized();
            commitTransaction(transaction);
            updateTxNotification(MO_TxNotification_Authorized);
            return;
        }

        MO_DBG_DEBUG("Abort transaction process (%s): timeout", transaction->getIdTag());
        transaction->setInactive();
        commitTransaction(transaction);
        updateTxNotification(MO_TxNotification_AuthorizationTimeout);
        return; //offline tx disabled
    });
    context.getMessageService().sendRequest(std::move(authorize));

    return transaction;
}

bool TransactionServiceEvse::beginTransaction_authorized(const char *idTag, const char *parentIdTag) {

    if (transaction) {
        MO_DBG_WARN("tx process still running. Please call endTransaction(...) before");
        return false;
    }

    transaction = allocateTransaction();

    if (!transaction) {
        MO_DBG_ERR("could not allocate Tx");
        return false;
    }

    if (!idTag || *idTag == '\0') {
        //input string is empty
        transaction->setIdTag("");
    } else {
        transaction->setIdTag(idTag);
    }

    if (parentIdTag) {
        transaction->setParentIdTag(parentIdTag);
    }

    transaction->setBeginTimestamp(clock.now());

    MO_DBG_DEBUG("Begin transaction process (%s), already authorized", idTag != nullptr ? idTag : "");

    transaction->setAuthorized();

    #if MO_ENABLE_RESERVATION
    if (model.getReservationService()) {
        if (auto reservation = model.getReservationService()->getReservation(evseId, idTag, parentIdTag)) {
            if (reservation->matches(idTag, parentIdTag)) {
                transaction->setReservationId(reservation->getReservationId());
            }
        }
    }
    #endif //MO_ENABLE_RESERVATION

    commitTransaction(transaction);

    return true;
}

bool TransactionServiceEvse::endTransaction(const char *idTag, const char *reason) {

    if (!transaction || !transaction->isActive()) {
        //transaction already ended / not active anymore
        return false;
    }

    MO_DBG_DEBUG("End session started by idTag %s",
                            transaction->getIdTag());

    if (!idTag || !strcmp(idTag, transaction->getIdTag())) {
        return endTransaction_authorized(idTag, reason);
    }

    const char *parentIdTag = transaction->getParentIdTag();
    if (strlen(parentIdTag) > 0)
    {
        // We have a parent ID tag, so we need to check if this new card also has one
        auto authorize = makeRequest(context, new Authorize(model, idTag));
        authorize->setTimeout(cService.authorizationTimeoutInt->getInt() > 0 ? cService.authorizationTimeoutInt->getInt(): 20);

        if (!connection->isConnected()) {
            //WebSockt unconnected. Enter offline mode immediately
            authorize->setTimeout(1);
        }

        auto txNr_capture = transaction->getTxNr();
        auto beginTimestamp_capture = transaction->getBeginTimestamp();
        auto idTag_capture = makeString(getMemoryTag(), idTag);
        auto reason_capture = makeString(getMemoryTag(), reason ? reason : "");
        authorize->setOnReceiveConf([this, txNr_capture, beginTimestamp_capture, idTag_capture, reason_capture] (JsonObject response) {

            int32_t dtBeginTimestamp = 0;
            if (!transaction || !context.getClock().delta(transaction->getBeginTimestamp(), beginTimestamp_capture, dtBeginTimestamp)) {
                dtBeginTimestamp = 0;
            }

            if (!transaction || transaction->getTxNr() != txNr_capture || dtBeginTimestamp != 0) {
                MO_DBG_DEBUG("stale Authorize");
                return;
            }

            JsonObject idTagInfo = response["idTagInfo"];

            if (strcmp("Accepted", idTagInfo["status"] | "_Undefined")) {
                //Authorization rejected, do nothing
                MO_DBG_DEBUG("Authorize rejected (%s), continue transaction", idTag_capture.c_str());
                updateTxNotification(MO_TxNotification_AuthorizationRejected);
                return;
            }
            if (idTagInfo.containsKey("parentIdTag") && *transaction->getParentIdTag() && !strcmp(idTagInfo["parenIdTag"] | "", transaction->getParentIdTag()))
            {
                endTransaction_authorized(idTag_capture.c_str(), reason_capture.empty() ? (const char*)nullptr : reason_capture.c_str());
            }
        });

        authorize->setOnTimeout([this, txNr_capture, beginTimestamp_capture, idTag_capture, reason_capture] () {
            //Authorization timed out, do nothing
            int32_t dtBeginTimestamp = 0;
            if (!transaction || !context.getClock().delta(transaction->getBeginTimestamp(), beginTimestamp_capture, dtBeginTimestamp)) {
                dtBeginTimestamp = 0;
            }

            if (!transaction || transaction->getTxNr() != txNr_capture || dtBeginTimestamp != 0) {
                MO_DBG_DEBUG("stale Authorize");
                return;
            }

            //check local OCPP whitelist
            const char *parentIdTag = nullptr; //locally stored parentIdTag

            #if MO_ENABLE_LOCAL_AUTH
            if (auto authService = model.getAuthorizationService()) {
                auto localAuth = authService->getLocalAuthorization(idTag_capture.c_str());

                //check authorization status
                if (localAuth && localAuth->getAuthorizationStatus() != AuthorizationStatus::Accepted) {
                    MO_DBG_DEBUG("local auth denied (%s)", idTag_capture.c_str());
                    localAuth = nullptr;
                }

                //check expiry
                int32_t dtExpiryDate;
                if (localAuth && localAuth->getExpiryDate() && clock.delta(clock.now(), *localAuth->getExpiryDate(), dtExpiryDate) && dtExpiryDate >= 0) {
                    MO_DBG_DEBUG("idTag %s local auth entry expired", idTag_capture.c_str());
                    localAuth = nullptr;
                }

                if (localAuth) {
                    parentIdTag = localAuth->getParentIdTag();
                }
            }
            #endif //MO_ENABLE_LOCAL_AUTH

            if (parentIdTag && *parentIdTag && !strcmp(parentIdTag, transaction->getParentIdTag())) {
                MO_DBG_DEBUG("IdTag locally authorized (%s)", idTag_capture.c_str());
                endTransaction_authorized(idTag_capture.c_str(), reason_capture.empty() ? (const char*)nullptr : reason_capture.c_str());
                return;
            }

            MO_DBG_DEBUG("Authorization timeout (%s), continue transaction", idTag_capture.c_str());
            updateTxNotification(MO_TxNotification_AuthorizationTimeout);
        });

        context.getMessageService().sendRequest(std::move(authorize));
        return true;
    } else {
        MO_DBG_INFO("endTransaction: idTag doesn't match");
        return false;
    }

    return false;
}

bool TransactionServiceEvse::endTransaction_authorized(const char *idTag, const char *reason) {

    if (!transaction || !transaction->isActive()) {
        //transaction already ended / not active anymore
        return false;
    }

    MO_DBG_DEBUG("End session started by idTag %s",
                            transaction->getIdTag());

    if (idTag && *idTag != '\0') {
        transaction->setStopIdTag(idTag);
    }

    if (reason) {
        transaction->setStopReason(reason);
    }
    transaction->setInactive();
    commitTransaction(transaction);
    return true;
}

Transaction *TransactionServiceEvse::getTransaction() {
    return transaction;
}

void TransactionServiceEvse::setConnectorPluggedInput(bool (*connectorPlugged)(unsigned int, void*), void *userData) {
    this->connectorPluggedInput = connectorPlugged;
    this->connectorPluggedInputUserData = userData;
}

void TransactionServiceEvse::setEvReadyInput(bool (*evReady)(unsigned int, void*), void *userData) {
    this->evReadyInput = evReady;
    this->evReadyInputUserData = userData;
}

void TransactionServiceEvse::setStartTxReadyInput(bool (*startTxReady)(unsigned int, void*), void *userData) {
    this->startTxReadyInput = startTxReady;
    this->startTxReadyInputUserData = userData;
}

void TransactionServiceEvse::setStopTxReadyInput(bool (*stopTxReady)(unsigned int, void*), void *userData) {
    this->stopTxReadyInput = stopTxReady;
    this->stopTxReadyInputUserData = userData;
}

void TransactionServiceEvse::setTxNotificationOutput(void (*txNotificationOutput)(MO_TxNotification, unsigned int, void*), void *userData) {
    this->txNotificationOutput = txNotificationOutput;
    this->txNotificationOutputUserData = userData;
}

void TransactionServiceEvse::updateTxNotification(MO_TxNotification event) {
    if (txNotificationOutput) {
        txNotificationOutput(event, evseId, txNotificationOutputUserData);
    }
}

unsigned int TransactionServiceEvse::getFrontRequestOpNr() {

    if (transactionFront && startTransactionInProgress) {
        return transactionFront->getStartSync().getOpNr();
    }

    if (transactionFront && stopTransactionInProgress) {
        return transactionFront->getStopSync().getOpNr();
    }

    /*
     * Advance front transaction?
     */
    unsigned int txSize = (txNrEnd + MO_TXNR_MAX - txNrFront) % MO_TXNR_MAX;

    if (transactionFront && txSize == 0) {
        //catch edge case where txBack has been rolled back and txFront was equal to txBack
        MO_DBG_DEBUG("collect front transaction %u-%u after tx rollback", evseId, transactionFront->getTxNr());
        MO_DBG_VERBOSE("txNrBegin=%u, txNrFront=%u, txNrEnd=%u", txNrBegin, txNrFront, txNrEnd);
        if (transactionFront != transaction) {
            delete transactionFront;
        }
        transactionFront = nullptr;
    }

    for (unsigned int i = 0; i < txSize; i++) {

        if (!transactionFront) {
            if (transaction && transaction->getTxNr() == txNrFront) {
                //back == front
                transactionFront = transaction; //share ownership

            } else if (filesystem) {

                auto txLoaded = new Transaction(evseId, txNrFront);
                if (!txLoaded) {
                    MO_DBG_ERR("OOM");
                    return NoOperation;
                }

                bool hardFailure = false;

                auto ret = TransactionStore::load(filesystem, context, evseId, txNrFront, *txLoaded);
                switch (ret) {
                    case FilesystemUtils::LoadStatus::Success:
                        //continue loading txMeterValues
                        break;
                    case FilesystemUtils::LoadStatus::FileNotFound:
                        break;
                    case FilesystemUtils::LoadStatus::ErrOOM:
                        MO_DBG_ERR("OOM");
                        hardFailure = true;
                        break;
                    case FilesystemUtils::LoadStatus::ErrFileCorruption:
                    case FilesystemUtils::LoadStatus::ErrOther:
                        MO_DBG_ERR("tx load failure");
                        break;
                }

                if (ret == FilesystemUtils::LoadStatus::Success) {
                    auto ret2 = MeterStore::load(filesystem, context, meteringServiceEvse->getMeterInputs(), evseId, txNrFront, txLoaded->getTxMeterValues());
                    switch (ret2) {
                        case FilesystemUtils::LoadStatus::Success:
                        case FilesystemUtils::LoadStatus::FileNotFound:
                            transactionFront = txLoaded;
                            txLoaded = nullptr;
                            break;
                        case FilesystemUtils::LoadStatus::ErrOOM:
                            MO_DBG_ERR("OOM");
                            hardFailure = true;
                            break;
                        case FilesystemUtils::LoadStatus::ErrFileCorruption:
                        case FilesystemUtils::LoadStatus::ErrOther:
                            MO_DBG_ERR("tx load failure");
                            MeterStore::remove(filesystem, evseId, txNrFront);
                            transactionFront = txLoaded;
                            txLoaded = nullptr;
                            break;
                    }
                }

                delete txLoaded;
                txLoaded = nullptr;

                if (hardFailure) {
                    return NoOperation;
                }

                if (transactionFront) {
                    MO_DBG_VERBOSE("load front transaction %u-%u", evseId, transactionFront->getTxNr());
                }
            }
        }

        if (transactionFront && (transactionFront->isAborted() || transactionFront->isCompleted() || transactionFront->isSilent())) {
            //advance front
            MO_DBG_DEBUG("collect front transaction %u-%u", evseId, transactionFront->getTxNr());
            if (transactionFront != transaction) {
                delete transactionFront;
            }
            transactionFront = nullptr;
            txNrFront = (txNrFront + 1) % MO_TXNR_MAX;
            MO_DBG_VERBOSE("txNrBegin=%u, txNrFront=%u, txNrEnd=%u", txNrBegin, txNrFront, txNrEnd);
        } else {
            //front is accurate. Done here
            break;
        }
    }

    if (transactionFront) {
        if (transactionFront->getStartSync().isRequested() && !transactionFront->getStartSync().isConfirmed()) {
            return transactionFront->getStartSync().getOpNr();
        }

        if (transactionFront->getStopSync().isRequested() && !transactionFront->getStopSync().isConfirmed()) {
            return transactionFront->getStopSync().getOpNr();
        }
    }

    return NoOperation;
}

std::unique_ptr<Request> TransactionServiceEvse::fetchFrontRequest() {

    if (!connection->isConnected()) {
        //offline behavior: pause sending messages and do not increment attempt counters
        return nullptr;
    }

    if (!clock.now().isUnixTime()) {
        //before unix time is set, do not process send queue. The attempt timestamps require absolute time
        return nullptr;
    }

    if (transactionFront && !transactionFront->isSilent()) {
        if (transactionFront->getStartSync().isRequested() && !transactionFront->getStartSync().isConfirmed()) {
            //send StartTx?

            if (startTransactionInProgress) {
                //ensure that only one StartTx request is being executed at the same time
                return nullptr;
            }

            bool cancelStartTx = false;

            //if not unix yet, convert (noop if unix time already)
            auto convertTime = transactionFront->getStartTimestamp();
            if (!clock.toUnixTime(transactionFront->getStartTimestamp(), convertTime)) {
                //time not set, cannot be restored anymore -> invalid tx
                MO_DBG_ERR("cannot recover tx from previus run");

                cancelStartTx = true;
            }
            transactionFront->setStartTimestamp(convertTime);

            if ((int)transactionFront->getStartSync().getAttemptNr() >= cService.transactionMessageAttemptsInt->getInt()) {
                MO_DBG_WARN("exceeded TransactionMessageAttempts. Discard transaction");

                cancelStartTx = true;
            }

            if (cancelStartTx) {
                transactionFront->setSilent();
                transactionFront->setInactive();
                commitTransaction(transactionFront);

                //clean up possible tx records
                if (filesystem) {
                    MeterStore::remove(filesystem, evseId, transactionFront->getTxNr());
                }
                //next getFrontRequestOpNr() call will collect transactionFront
                return nullptr;
            }

            int32_t dtLastAttempt;
            if (!clock.delta(clock.now(), transactionFront->getStartSync().getAttemptTime(), dtLastAttempt)) {
                MO_DBG_ERR("internal error");
                dtLastAttempt = 0;
            }

            if (dtLastAttempt < (int)transactionFront->getStartSync().getAttemptNr() * std::max(0, cService.transactionMessageRetryIntervalInt->getInt())) {
                return nullptr;
            }

            transactionFront->getStartSync().advanceAttemptNr();
            transactionFront->getStartSync().setAttemptTime(clock.now());
            commitTransaction(transactionFront);

            auto startTxOp = new StartTransaction(context, transactionFront);
            if (!startTxOp) {
                MO_DBG_ERR("OOM");
                return nullptr;
            }

            auto startTx = makeRequest(context, startTxOp);
            if (!startTx) {
                MO_DBG_ERR("OOM");
                return nullptr;
            }
            startTx->setOnReceiveConf([this] (JsonObject response) {
                //fetch authorization status from StartTransaction.conf() for user notification
                commitTransaction(transactionFront);
                startTransactionInProgress = false;

                const char* idTagInfoStatus = response["idTagInfo"]["status"] | "_Undefined";
                if (strcmp(idTagInfoStatus, "Accepted")) {
                    if (transactionFront == transaction) { //check if the front transaction is the currently active transaciton
                        updateTxNotification(MO_TxNotification_DeAuthorized);
                    }
                }
            });
            startTx->setOnAbort([this] () {
                //shortcut to the attemptNr check above. Relevant if other operations block the queue while this StartTx is timing out

                startTransactionInProgress = false;

                if ((int)transactionFront->getStartSync().getAttemptNr() >= cService.transactionMessageAttemptsInt->getInt()) {
                    MO_DBG_WARN("exceeded TransactionMessageAttempts. Discard transaction");

                    transactionFront->setSilent();
                    transactionFront->setInactive();
                    commitTransaction(transactionFront);

                    //clean up possible tx records
                    if (filesystem) {
                        MeterStore::remove(filesystem, evseId, transactionFront->getTxNr());
                    }
                    //next getFrontRequestOpNr() call will collect transactionFront
                }
            });

            startTransactionInProgress = true;

            return startTx;
        }

        if (transactionFront->getStopSync().isRequested() && !transactionFront->getStopSync().isConfirmed()) {
            //send StopTx?

            if (stopTransactionInProgress) {
                //ensure that only one TransactionEvent request is being executed at the same time
                return nullptr;
            }

            if ((int)transactionFront->getStopSync().getAttemptNr() >= cService.transactionMessageAttemptsInt->getInt()) {
                MO_DBG_WARN("exceeded TransactionMessageAttempts. Discard transaction");

                transactionFront->setSilent();

                //clean up possible tx records
                if (filesystem) {
                    MeterStore::remove(filesystem, evseId, transactionFront->getTxNr());
                }
                //next getFrontRequestOpNr() call will collect transactionFront
                return nullptr;
            }

            bool fallbackStopTime = false;

            //if not unix yet, convert (noop if unix time already)
            auto convertTime = transactionFront->getStopTimestamp();
            if (!clock.toUnixTime(transactionFront->getStopTimestamp(), convertTime)) {
                //fall back to start timestamp
                MO_DBG_ERR("cannot recover tx from previus run");
                fallbackStopTime = true;
            }
            transactionFront->setStopTimestamp(convertTime);

            int32_t dt;
            if (clock.delta(transactionFront->getStopTimestamp(), transactionFront->getStartTimestamp(), dt) &&
                    dt < 0) {
                MO_DBG_ERR("StopTx time is before StartTx time");
                fallbackStopTime = true;
            }

            if (fallbackStopTime) {
                transactionFront->setStopTimestamp(transactionFront->getStartTimestamp());
                auto timestamp = transactionFront->getStopTimestamp();
                clock.add(timestamp, 1); //set 1 second after start timestamp
                transactionFront->setStopTimestamp(timestamp);
            }

            //check timestamps in tx meter values
            auto& txMeterValues = transactionFront->getTxMeterValues();
            for (size_t i = 0; i < txMeterValues.size(); i++) {
                auto mv = txMeterValues[i];

                char dummy [MO_JSONDATE_SIZE];
                if (!context.getClock().toJsonString(mv->timestamp, dummy, sizeof(dummy))) {
                    //try to fix timestamp
                    if (mv->readingContext == MO_ReadingContext_TransactionBegin) {
                        mv->timestamp = transaction->getStartTimestamp();
                    } else if (mv->readingContext == MO_ReadingContext_TransactionEnd) {
                        mv->timestamp = transaction->getStopTimestamp();
                    } else {
                        mv->timestamp = transaction->getStartTimestamp();
                        clock.add(mv->timestamp, 1 + (int)i);
                    }
                }
            }

            int32_t dtLastAttempt;
            if (!clock.delta(clock.now(), transactionFront->getStopSync().getAttemptTime(), dtLastAttempt)) {
                MO_DBG_ERR("internal error");
                dtLastAttempt = 0;
            }

            if (dtLastAttempt < (int)transactionFront->getStopSync().getAttemptNr() * std::max(0, cService.transactionMessageRetryIntervalInt->getInt())) {
                return nullptr;
            }

            transactionFront->getStopSync().advanceAttemptNr();
            transactionFront->getStopSync().setAttemptTime(clock.now());
            commitTransaction(transactionFront);

            auto stopTxOp = new StopTransaction(context, transactionFront);
            if (!stopTxOp) {
                MO_DBG_ERR("OOM");
                return nullptr;
            }

            auto stopTx = makeRequest(context, stopTxOp);
            if (!stopTx) {
                MO_DBG_ERR("OOM");
                return nullptr;
            }
            stopTx->setOnReceiveConf([this] (JsonObject response) {
                commitTransaction(transactionFront);
                stopTransactionInProgress = false;
            });
            stopTx->setOnAbort([this] () {
                stopTransactionInProgress = false;

                //shortcut to the attemptNr check above. Relevant if other operations block the queue while this StopTx is timing out
                if ((int)transactionFront->getStopSync().getAttemptNr() >= cService.transactionMessageAttemptsInt->getInt()) {
                    MO_DBG_WARN("exceeded TransactionMessageAttempts. Discard transaction");

                    transactionFront->setSilent();
                    transactionFront->setInactive();
                    commitTransaction(transactionFront);

                    //clean up possible tx records
                    if (filesystem) {
                        MeterStore::remove(filesystem, evseId, transactionFront->getTxNr());
                    }
                    //next getFrontRequestOpNr() call will collect transactionFront
                }
            });

            stopTransactionInProgress = true;

            return stopTx;
        }
    }

    return nullptr;
}

unsigned int TransactionServiceEvse::getTxNrBeginHistory() {
    return txNrBegin;
}

unsigned int TransactionServiceEvse::getTxNrFront() {
    return txNrFront;
}

unsigned int TransactionServiceEvse::getTxNrEnd() {
    return txNrEnd;
}

Transaction *TransactionServiceEvse::getTransactionFront() {
    return transactionFront;
}

TransactionService::TransactionService(Context& context) :
        MemoryManaged("v16.Transactions.TransactionService"), context(context) {

}

TransactionService::~TransactionService() {
    for (unsigned int i = 0; i < MO_NUM_EVSEID; i++) {
        delete evses[i];
        evses[i] = nullptr;
    }
}

bool TransactionService::setup() {

    auto configService = context.getModel16().getConfigurationService();
    if (!configService) {
        MO_DBG_ERR("setup failure");
        return false;
    }

    numEvseId = context.getModel16().getNumEvseId();

    configService->declareConfiguration<int>("NumberOfConnectors", numEvseId >= 1 ? numEvseId - 1 : 0, MO_CONFIGURATION_VOLATILE, Mutability::ReadOnly);

    connectionTimeOutInt = configService->declareConfiguration<int>("ConnectionTimeOut", 30);
    configService->registerValidator<int>("ConnectionTimeOut", VALIDATE_UNSIGNED_INT);
    configService->registerValidator<int>("MinimumStatusDuration", VALIDATE_UNSIGNED_INT);
    stopTransactionOnInvalidIdBool = configService->declareConfiguration<bool>("StopTransactionOnInvalidId", true);
    stopTransactionOnEVSideDisconnectBool = configService->declareConfiguration<bool>("StopTransactionOnEVSideDisconnect", true);
    localPreAuthorizeBool = configService->declareConfiguration<bool>("LocalPreAuthorize", false);
    localAuthorizeOfflineBool = configService->declareConfiguration<bool>("LocalAuthorizeOffline", true);
    allowOfflineTxForUnknownIdBool = configService->declareConfiguration<bool>("AllowOfflineTxForUnknownId", false);

    //if the EVSE goes offline, can it continue to charge without sending StartTx / StopTx to the server when going online again?
    silentOfflineTransactionsBool = configService->declareConfiguration<bool>(MO_CONFIG_EXT_PREFIX "SilentOfflineTransactions", false);

    //how long the EVSE tries the Authorize request before it enters offline mode
    authorizationTimeoutInt = configService->declareConfiguration<int>(MO_CONFIG_EXT_PREFIX "AuthorizationTimeout", 20);
    configService->registerValidator<int>(MO_CONFIG_EXT_PREFIX "AuthorizationTimeout", VALIDATE_UNSIGNED_INT);

    //FreeVend mode
    freeVendActiveBool = configService->declareConfiguration<bool>(MO_CONFIG_EXT_PREFIX "FreeVendActive", false);
    freeVendIdTagString = configService->declareConfiguration<const char*>(MO_CONFIG_EXT_PREFIX "FreeVendIdTag", "");

    txStartOnPowerPathClosedBool = configService->declareConfiguration<bool>(MO_CONFIG_EXT_PREFIX "TxStartOnPowerPathClosed", false);

    transactionMessageAttemptsInt = configService->declareConfiguration<int>("TransactionMessageAttempts", 3);
    configService->registerValidator<int>("TransactionMessageAttempts", VALIDATE_UNSIGNED_INT);
    transactionMessageRetryIntervalInt = configService->declareConfiguration<int>("TransactionMessageRetryInterval", 60);
    configService->registerValidator<int>("TransactionMessageRetryInterval", VALIDATE_UNSIGNED_INT);

    if (!connectionTimeOutInt ||
            !stopTransactionOnInvalidIdBool ||
            !stopTransactionOnEVSideDisconnectBool ||
            !localPreAuthorizeBool ||
            !localAuthorizeOfflineBool ||
            !allowOfflineTxForUnknownIdBool ||
            !silentOfflineTransactionsBool ||
            !authorizationTimeoutInt ||
            !freeVendActiveBool ||
            !freeVendIdTagString ||
            !txStartOnPowerPathClosedBool ||
            !transactionMessageAttemptsInt ||
            !transactionMessageRetryIntervalInt) {

        MO_DBG_ERR("declareConfiguration failed");
        return false;
    }

    context.getMessageService().registerOperation("ClearCache", [] (Context& context) -> Operation* {
        return new ClearCache(context.getFilesystem());});

    #if MO_ENABLE_MOCK_SERVER
    context.getMessageService().registerOperation("Authorize", nullptr, Authorize::writeMockConf);
    context.getMessageService().registerOperation("StartTransaction", nullptr, StartTransaction::writeMockConf);
    context.getMessageService().registerOperation("StopTransaction", nullptr, StopTransaction::writeMockConf);
    #endif //MO_ENABLE_MOCK_SERVER

    for (unsigned int i = 0; i < numEvseId; i++) {
        if (!getEvse(i) || !getEvse(i)->setup()) {
            MO_DBG_ERR("connector init failure");
            return false;
        }
    }

    return true;
}

void TransactionService::loop() {
    for (unsigned int i = 0; i < numEvseId; i++) {
        evses[i]->loop();
    }
}

TransactionServiceEvse* TransactionService::getEvse(unsigned int evseId) {
    if (evseId >= numEvseId) {
        MO_DBG_ERR("evseId out of bound");
        return nullptr;
    }

    if (!evses[evseId]) {
        evses[evseId] = new TransactionServiceEvse(context, *this, evseId);
        if (!evses[evseId]) {
            MO_DBG_ERR("OOM");
            return nullptr;
        }
    }

    return evses[evseId];
}
