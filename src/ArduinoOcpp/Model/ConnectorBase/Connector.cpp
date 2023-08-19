// matth-x/ArduinoOcpp
// Copyright Matthias Akstaller 2019 - 2023
// MIT License

#include <ArduinoOcpp/Model/ChargeControl/Connector.h>

#include <ArduinoOcpp/Core/Context.h>
#include <ArduinoOcpp/Model/Model.h>
#include <ArduinoOcpp/Core/Request.h>
#include <ArduinoOcpp/Model/Transactions/TransactionStore.h>
#include <ArduinoOcpp/Core/Configuration.h>

#include <ArduinoOcpp/Operations/Authorize.h>
#include <ArduinoOcpp/Operations/StatusNotification.h>
#include <ArduinoOcpp/Operations/StartTransaction.h>
#include <ArduinoOcpp/Operations/StopTransaction.h>
#include <ArduinoOcpp/Operations/CiStrings.h>

#include <ArduinoOcpp/Debug.h>

#include <ArduinoOcpp/Model/Metering/MeteringService.h>
#include <ArduinoOcpp/Model/Reservation/ReservationService.h>
#include <ArduinoOcpp/Model/Authorization/AuthorizationService.h>

#include <ArduinoOcpp/Core/SimpleRequestFactory.h>

using namespace ArduinoOcpp;
using namespace ArduinoOcpp::Ocpp16;

Connector::Connector(Context& context, int connectorId)
        : context(context), model(context.getModel()), connectorId{connectorId} {

    char availabilityKey [CONF_KEYLEN_MAX + 1] = {'\0'};
    snprintf(availabilityKey, CONF_KEYLEN_MAX + 1, "AO_AVAIL_CONN_%d", connectorId);
    availability = declareConfiguration<bool>(availabilityKey, true, AO_KEYVALUE_FN, false, false, true, false);

    connectionTimeOut = declareConfiguration<int>("ConnectionTimeOut", 30, CONFIGURATION_FN, true, true, true, false);
    minimumStatusDuration = declareConfiguration<int>("MinimumStatusDuration", 0, CONFIGURATION_FN, true, true, true, false);
    stopTransactionOnInvalidId = declareConfiguration<bool>("StopTransactionOnInvalidId", true, CONFIGURATION_FN, true, true, true, false);
    stopTransactionOnEVSideDisconnect = declareConfiguration<bool>("StopTransactionOnEVSideDisconnect", true, CONFIGURATION_FN, true, true, true, false);
    unlockConnectorOnEVSideDisconnect = declareConfiguration<bool>("UnlockConnectorOnEVSideDisconnect", true, CONFIGURATION_FN, true, true, true, false);
    localPreAuthorize = declareConfiguration<bool>("LocalPreAuthorize", false, CONFIGURATION_FN, true, true, true, false);
    allowOfflineTxForUnknownId = ArduinoOcpp::declareConfiguration<bool>("AllowOfflineTxForUnknownId", false, CONFIGURATION_FN);

    //if the EVSE goes offline, can it continue to charge without sending StartTx / StopTx to the server when going online again?
    silentOfflineTransactions = declareConfiguration<bool>("AO_SilentOfflineTransactions", false, CONFIGURATION_FN, true, true, true, false);

    //how long the EVSE tries the Authorize request before it enters offline mode
    authorizationTimeout = ArduinoOcpp::declareConfiguration<int>("AO_AuthorizationTimeout", 20, CONFIGURATION_FN);

    //FreeVend mode
    freeVendActive = declareConfiguration<bool>("AO_FreeVendActive", false, CONFIGURATION_FN, true, true, true, false);
    freeVendIdTag = declareConfiguration<const char*>("AO_FreeVendIdTag", "", CONFIGURATION_FN, true, true, true, false);

    if (!availability) {
        AO_DBG_ERR("Cannot declare availability");
    }

    if (model.getTransactionStore()) {
        transaction = model.getTransactionStore()->getLatestTransaction(connectorId);
    } else {
        AO_DBG_ERR("must initialize TxStore before Connector");
        (void)0;
    }
}

ChargePointStatus Connector::getStatus() {
    /*
    * Handle special case: This is the Connector for the whole CP (i.e. connectorId=0) --> only states Available, Unavailable, Faulted are possible
    */
    if (connectorId == 0) {
        if (isFaulted()) {
            return ChargePointStatus::Faulted;
        } else if (!isOperative()) {
            return ChargePointStatus::Unavailable;
        } else {
            return ChargePointStatus::Available;
        }
    }

    if (isFaulted()) {
        return ChargePointStatus::Faulted;
    } else if (!isOperative()) {
        return ChargePointStatus::Unavailable;
    } else if (transaction && transaction->isRunning()) {
        //Transaction is currently running
        if (connectorPluggedInput && !connectorPluggedInput()) { //special case when StopTransactionOnEVSideDisconnect is false
            return ChargePointStatus::SuspendedEV;
        }
        if (!ocppPermitsCharge() ||
                (evseReadyInput && !evseReadyInput())) { 
            return ChargePointStatus::SuspendedEVSE;
        }
        if (evReadyInput && !evReadyInput()) {
            return ChargePointStatus::SuspendedEV;
        }
        return ChargePointStatus::Charging;
    } else if (model.getReservationService() && model.getReservationService()->getReservation(connectorId)) {
        return ChargePointStatus::Reserved;
    } else if ((!transaction || !transaction->isActive()) &&                 //no transaction preparation
               (!connectorPluggedInput || !connectorPluggedInput()) &&   //no vehicle plugged
               (!occupiedInput || !occupiedInput())) {                       //occupied override clear
        return ChargePointStatus::Available;
    } else {
        /*
         * Either in Preparing or Finishing state. Only way to know is from previous state
         */
        const auto previous = currentStatus;
        if (previous == ChargePointStatus::Finishing ||
                previous == ChargePointStatus::Charging ||
                previous == ChargePointStatus::SuspendedEV ||
                previous == ChargePointStatus::SuspendedEVSE) {
            return ChargePointStatus::Finishing;
        } else {
            return ChargePointStatus::Preparing;
        }
    }

    AO_DBG_DEBUG("status undefined");
    return ChargePointStatus::Faulted; //internal error
}

bool Connector::ocppPermitsCharge() {
    if (connectorId == 0) {
        AO_DBG_WARN("not supported for connectorId == 0");
        return false;
    }

    bool suspendDeAuthorizedIdTag = transaction && transaction->isIdTagDeauthorized(); //if idTag status is "DeAuthorized" and if charging should stop
    
    //check special case for DeAuthorized idTags: FreeVend mode
    if (suspendDeAuthorizedIdTag && freeVendActive && *freeVendActive) {
        suspendDeAuthorizedIdTag = false;
    }

    return transaction &&
            transaction->isRunning() &&
            transaction->isActive() &&
            !isFaulted() &&
            !suspendDeAuthorizedIdTag;
}

void Connector::loop() {

    if (!trackLoopExecute) {
        trackLoopExecute = true;
    }

    if (transaction && transaction->isAborted()) {
        //If the transaction is aborted (invalidated before started), delete all artifacts from flash
        //This is an optimization. The memory management will attempt to remove those files again later
        bool removed = true;
        if (auto mService = model.getMeteringService()) {
            removed &= mService->removeTxMeterData(connectorId, transaction->getTxNr());
        }

        if (removed) {
            removed &= model.getTransactionStore()->remove(connectorId, transaction->getTxNr());
        }

        if (removed) {
            model.getTransactionStore()->setTxEnd(connectorId, transaction->getTxNr()); //roll back creation of last tx entry
        }

        AO_DBG_DEBUG("collect aborted transaction %u-%u %s", connectorId, transaction->getTxNr(), removed ? "" : "failure");
        transaction = nullptr;
    }

    if (transaction && transaction->isCompleted()) {
        AO_DBG_DEBUG("collect obsolete transaction %u-%u", connectorId, transaction->getTxNr());
        transaction = nullptr;
    }

    if (transaction) { //begin exclusively transaction-related operations
            
        if (connectorPluggedInput) {
            if (transaction->isRunning() && transaction->isActive() && !connectorPluggedInput()) {
                if (!stopTransactionOnEVSideDisconnect || *stopTransactionOnEVSideDisconnect) {
                    AO_DBG_DEBUG("Stop Tx due to EV disconnect");
                    transaction->setStopReason("EVDisconnected");
                    transaction->setInactive();
                    transaction->commit();
                }
            }
        }

        if (transaction->isActive() &&
                !transaction->getStartSync().isRequested() &&
                transaction->getBeginTimestamp() > MIN_TIME &&
                connectionTimeOut && *connectionTimeOut > 0 &&
                model.getClock().now() - transaction->getBeginTimestamp() >= *connectionTimeOut) {
                
            AO_DBG_INFO("Session mngt: timeout");
            transaction->setInactive();
            transaction->commit();

            updateTxNotification(TxNotification::ConnectionTimeout);
        }

        if (transaction->isActive() &&
                transaction->isIdTagDeauthorized() && ( //transaction has been deAuthorized
                    !transaction->isRunning() ||        //if transaction hasn't started yet, always end
                    !stopTransactionOnInvalidId || *stopTransactionOnInvalidId)) { //if transaction is running, behavior depends on StopTransactionOnInvalidId
            
            AO_DBG_DEBUG("DeAuthorize session");
            transaction->setStopReason("DeAuthorized");
            transaction->setInactive();
            transaction->commit();
        }

        /*
        * Check conditions for start or stop transaction
        */

        if (!transaction->isRunning()) {
            //start tx?

            if (transaction->isActive() && transaction->isAuthorized() &&  //tx must be authorized
                    (!connectorPluggedInput || connectorPluggedInput()) && //if applicable, connector must be plugged
                    isOperative() && //only start tx if charger is free of error conditions
                    (!startTxReadyInput || startTxReadyInput())) { //if defined, user Input for allowing StartTx must be true
                //start Transaction

                AO_DBG_INFO("Session mngt: trigger StartTransaction");

                auto meteringService = model.getMeteringService();
                if (transaction->getMeterStart() < 0 && meteringService) {
                    auto meterStart = meteringService->readTxEnergyMeter(transaction->getConnectorId(), ReadingContext::TransactionBegin);
                    if (meterStart && *meterStart) {
                        transaction->setMeterStart(meterStart->toInteger());
                    } else {
                        AO_DBG_ERR("MeterStart undefined");
                    }
                }

                if (transaction->getStartTimestamp() <= MIN_TIME) {
                    transaction->setStartTimestamp(model.getClock().now());
                    transaction->setStartBootNr(model.getBootNr());
                }

                updateTxNotification(TxNotification::StartTx);

                if (transaction->isSilent()) {
                    AO_DBG_INFO("silent Transaction: omit StartTx");
                    transaction->getStartSync().setRequested();
                    transaction->getStartSync().confirm();
                    transaction->commit();
                    return;
                }

                transaction->commit();

                if (model.getMeteringService()) {
                    model.getMeteringService()->beginTxMeterData(transaction.get());
                }

                auto startTx = makeRequest(new StartTransaction(model, transaction));
                startTx->setTimeout(0);
                startTx->setOnReceiveConfListener([this] (JsonObject response) {
                    //fetch authorization status from StartTransaction.conf() for user notification

                    const char* idTagInfoStatus = response["idTagInfo"]["status"] | "_Undefined";
                    if (strcmp(idTagInfoStatus, "Accepted")) {
                        updateTxNotification(TxNotification::DeAuthorized);
                    }
                });
                context.initiateRequest(std::move(startTx));
                return;
            }
        } else  {
            //stop tx?

            if (!transaction->isActive() &&
                    (!stopTxReadyInput || stopTxReadyInput())) {
                //stop transaction

                AO_DBG_INFO("Session mngt: trigger StopTransaction");

                if (transaction->isSilent()) {
                    AO_DBG_INFO("silent Transaction: omit StopTx");
                    updateTxNotification(TxNotification::StopTx);
                    transaction->getStopSync().setRequested();
                    transaction->getStopSync().confirm();
                    if (auto mService = model.getMeteringService()) {
                        mService->removeTxMeterData(connectorId, transaction->getTxNr());
                    }
                    model.getTransactionStore()->remove(connectorId, transaction->getTxNr());
                    model.getTransactionStore()->setTxEnd(connectorId, transaction->getTxNr());
                    transaction = nullptr;
                    return;
                }
                
                auto meteringService = model.getMeteringService();
                if (transaction->getMeterStop() < 0 && meteringService) {
                    auto meterStop = meteringService->readTxEnergyMeter(transaction->getConnectorId(), ReadingContext::TransactionEnd);
                    if (meterStop && *meterStop) {
                        transaction->setMeterStop(meterStop->toInteger());
                    } else {
                        AO_DBG_ERR("MeterStop undefined");
                    }
                }

                if (transaction->getStopTimestamp() <= MIN_TIME) {
                    transaction->setStopTimestamp(model.getClock().now());
                    transaction->setStopBootNr(model.getBootNr());
                }

                transaction->commit();

                updateTxNotification(TxNotification::StopTx);

                std::shared_ptr<TransactionMeterData> stopTxData;

                if (meteringService) {
                    stopTxData = meteringService->endTxMeterData(transaction.get());
                }

                std::unique_ptr<Request> stopTx;

                if (stopTxData) {
                    stopTx = makeRequest(new StopTransaction(model, std::move(transaction), stopTxData->retrieveStopTxData()));
                } else {
                    stopTx = makeRequest(new StopTransaction(model, std::move(transaction)));
                }
                stopTx->setTimeout(0);
                context.initiateRequest(std::move(stopTx));
                return;
            }
        }
    } //end transaction-related operations

    //handle FreeVend mode
    if (freeVendActive && *freeVendActive && connectorPluggedInput) {
        if (!freeVendTrackPlugged && connectorPluggedInput() && !transaction) {
            const char *idTag = freeVendIdTag && *freeVendIdTag ? *freeVendIdTag : "";
            if (!idTag || *idTag == '\0') {
                idTag = "A0000000";
            }
            AO_DBG_INFO("begin FreeVend Tx using idTag %s", idTag);
            beginTransaction_authorized(idTag);
            
            if (!transaction) {
                AO_DBG_ERR("could not begin FreeVend Tx");
                (void)0;
            }
        }

        freeVendTrackPlugged = connectorPluggedInput();
    }

    auto status = getStatus();

    for (auto i = std::min(errorDataInputs.size(), trackErrorDataInputs.size()); i >= 1; i--) {
        auto index = i - 1;
        auto error = errorDataInputs[index].operator()();
        if (error.isError && !trackErrorDataInputs[index]) {
            //new error
            auto statusNotification = makeRequest(
                    new StatusNotification(connectorId, status, model.getClock().now(), error));
            statusNotification->setTimeout(0);
            context.initiateRequest(std::move(statusNotification));

            currentStatus = status;
            reportedStatus = status;
            trackErrorDataInputs[index] = true;
        } else if (!error.isError && trackErrorDataInputs[index]) {
            //reset error
            trackErrorDataInputs[index] = false;
        }
    }
    
    if (status != currentStatus) {
        currentStatus = status;
        t_statusTransition = ao_tick_ms();
        AO_DBG_DEBUG("Status changed%s", *minimumStatusDuration ? ", will report delayed" : "");
    }

    if (reportedStatus != currentStatus &&
            model.getClock().now() >= Timestamp(2010,0,0,0,0,0) &&
            (*minimumStatusDuration <= 0 || //MinimumStatusDuration disabled
            ao_tick_ms() - t_statusTransition >= ((unsigned long) *minimumStatusDuration) * 1000UL)) {
        reportedStatus = currentStatus;
        Timestamp reportedTimestamp = model.getClock().now();
        reportedTimestamp -= (ao_tick_ms() - t_statusTransition) / 1000UL;

        auto statusNotification = makeRequest(
                new StatusNotification(connectorId, reportedStatus, reportedTimestamp, getErrorCode()));
        statusNotification->setTimeout(0);
        context.initiateRequest(std::move(statusNotification));
        return;
    }

    return;
}

bool Connector::isFaulted() {
    //for (auto i = errorDataInputs.begin(); i != errorDataInputs.end(); ++i) {
    for (size_t i = 0; i < errorDataInputs.size(); i++) {
        if (errorDataInputs[i].operator()().isFaulted) {
            return true;
        }
    }
    return false;
}

const char *Connector::getErrorCode() {
    for (auto i = errorDataInputs.size(); i >= 1; i--) {
        auto error = errorDataInputs[i-1].operator()();
        if (error.isError && error.errorCode) {
            return error.errorCode;
        }
    }
    return nullptr;
}

std::shared_ptr<Transaction> Connector::allocateTransaction() {

    decltype(allocateTransaction()) tx;

    //clean possible aorted tx
    auto txr = model.getTransactionStore()->getTxEnd(connectorId);
    auto txsize = model.getTransactionStore()->size(connectorId);
    for (decltype(txsize) i = 0; i < txsize; i++) {
        txr = (txr + MAX_TX_CNT - 1) % MAX_TX_CNT; //decrement by 1
        
        auto tx = model.getTransactionStore()->getTransaction(connectorId, txr);
        //check if dangling silent tx, aborted tx, or corrupted entry (tx == null)
        if (!tx || tx->isSilent() || tx->isAborted()) {
            //yes, remove
            bool removed = true;
            if (auto mService = model.getMeteringService()) {
                removed &= mService->removeTxMeterData(connectorId, txr);
            }
            if (removed) {
                removed &= model.getTransactionStore()->remove(connectorId, txr);
            }
            if (removed) {
                model.getTransactionStore()->setTxEnd(connectorId, txr);
                AO_DBG_WARN("deleted dangling silent or aborted tx for new transaction");
            } else {
                AO_DBG_ERR("memory corruption");
                break;
            }
        } else {
            //no, tx record trimmed, end
            break;
        }
    }

    //try to create new transaction
    tx = model.getTransactionStore()->createTransaction(connectorId);

    if (!tx) {
        //could not create transaction - now, try to replace tx history entry

        auto txl = model.getTransactionStore()->getTxBegin(connectorId);
        auto txsize = model.getTransactionStore()->size(connectorId);

        for (decltype(txsize) i = 0; i < txsize; i++) {

            if (tx) {
                //success, finished here
                break;
            }

            //no transaction allocated, delete history entry to make space

            auto txhist = model.getTransactionStore()->getTransaction(connectorId, txl);
            //oldest entry, now check if it's history and can be removed or corrupted entry
            if (!txhist || txhist->isCompleted()) {
                //yes, remove
                bool removed = true;
                if (auto mService = model.getMeteringService()) {
                    removed &= mService->removeTxMeterData(connectorId, txl);
                }
                if (removed) {
                    removed &= model.getTransactionStore()->remove(connectorId, txl);
                }
                if (removed) {
                    model.getTransactionStore()->setTxBegin(connectorId, (txl + 1) % MAX_TX_CNT);
                    AO_DBG_DEBUG("deleted tx history entry for new transaction");

                    tx = model.getTransactionStore()->createTransaction(connectorId);
                } else {
                    AO_DBG_ERR("memory corruption");
                    break;
                }
            } else {
                //no, end of history reached, don't delete further tx
                AO_DBG_DEBUG("cannot delete more tx");
                break;
            }

            txl++;
            txl %= MAX_TX_CNT;
        }
    }

    if (!tx) {
        //couldn't create normal transaction -> check if to start charging without real transaction
        if (silentOfflineTransactions && *silentOfflineTransactions) {
            //try to handle charging session without sending StartTx or StopTx to the server
            tx = model.getTransactionStore()->createTransaction(connectorId, true);

            if (tx) {
                AO_DBG_DEBUG("created silent transaction");
                (void)0;
            }
        }
    }

    return tx;
}

std::shared_ptr<Transaction> Connector::beginTransaction(const char *idTag) {

    if (transaction) {
        AO_DBG_WARN("tx process still running. Please call endTransaction(...) before");
        return nullptr;
    }

    auto rService = model.getReservationService();
    AuthorizationData *localAuth = nullptr;
    bool expiredLocalAuth = false;

    //check local OCPP whitelist
    if (auto authService = model.getAuthorizationService()) {
        
        localAuth = authService->getLocalAuthorization(idTag);

        //check if blocked by reservation
        Reservation *reservation = nullptr;
        if (localAuth && rService) {
            reservation = rService->getReservation(
                        connectorId,
                        idTag,
                        localAuth->getParentIdTag() ? localAuth->getParentIdTag() : nullptr);
            if (reservation && !reservation->matches(
                        idTag,
                        localAuth->getParentIdTag() ? localAuth->getParentIdTag() : nullptr)) {
                //reservation found for connector but does not match idTag or parentIdTag
                AO_DBG_DEBUG("reservation blocks local auth at connector %u", connectorId);
                localAuth = nullptr;;
            }
        }

        if (localAuth && localAuth->getAuthorizationStatus() != AuthorizationStatus::Accepted) {
            AO_DBG_DEBUG("local auth denied (%s)", idTag);
            localAuth = nullptr;
        }

        //check expiry
        if (localAuth && localAuth->getExpiryDate()) {
            auto& tnow = model.getClock().now();
            if (tnow >= *localAuth->getExpiryDate()) {
                AO_DBG_DEBUG("idTag %s local auth entry expired", idTag);
                localAuth = nullptr;
                expiredLocalAuth = true;
            }
        }
    }

    transaction = allocateTransaction();

    if (!transaction) {
        AO_DBG_ERR("could not allocate Tx");
        return nullptr;
    }

    if (!idTag || *idTag == '\0') {
        //input string is empty
        transaction->setIdTag("");
    } else {
        transaction->setIdTag(idTag);
    }

    transaction->setBeginTimestamp(model.getClock().now());

    AO_DBG_DEBUG("Begin transaction process (%s), prepare", idTag != nullptr ? idTag : "");

    //check for local preauthorization
    if (localPreAuthorize && *localPreAuthorize) {
        if (localAuth && localAuth->getAuthorizationStatus() == AuthorizationStatus::Accepted) {
            AO_DBG_DEBUG("Begin transaction process (%s), preauthorized locally", idTag != nullptr ? idTag : "");

            transaction->setAuthorized();

            updateTxNotification(TxNotification::Authorized);
        }
    }

    transaction->commit();

    auto authorize = makeRequest(new Authorize(context.getModel(), idTag));
    authorize->setTimeout(authorizationTimeout && *authorizationTimeout > 0 ? *authorizationTimeout * 1000UL : 20UL * 1000UL);
    auto tx = transaction;
    authorize->setOnReceiveConfListener([this, tx] (JsonObject response) {
        JsonObject idTagInfo = response["idTagInfo"];

        if (strcmp("Accepted", idTagInfo["status"] | "UNDEFINED")) {
            //Authorization rejected, abort transaction
            AO_DBG_DEBUG("Authorize rejected (%s), abort tx process", tx->getIdTag());
            tx->setIdTagDeauthorized();
            tx->commit();
            updateTxNotification(TxNotification::AuthorizationRejected);
            return;
        }

        if (model.getReservationService()) {
            auto reservation = model.getReservationService()->getReservation(
                        connectorId,
                        tx->getIdTag(),
                        idTagInfo["parentIdTag"] | (const char*) nullptr);
            if (reservation && !reservation->matches(
                        tx->getIdTag(),
                        idTagInfo["parentIdTag"] | (const char*) nullptr)) {
                //reservation found for connector but does not match idTag or parentIdTag
                AO_DBG_INFO("connector %u reserved - abort transaction", connectorId);
                tx->setInactive();
                tx->commit();
                updateTxNotification(TxNotification::ReservationConflict);
                return;
            }
        }

        AO_DBG_DEBUG("Authorized transaction process (%s)", tx->getIdTag());
        tx->setAuthorized();
        tx->commit();

        updateTxNotification(TxNotification::Authorized);
    });
    authorize->setOnTimeoutListener([this, tx, localAuth, expiredLocalAuth] () {

        if (expiredLocalAuth) {
            //local auth entry exists, but is expired -> avoid offline tx
            AO_DBG_DEBUG("Abort transaction process (%s), timeout, expired local auth", tx->getIdTag());
            tx->setInactive();
            tx->commit();
            updateTxNotification(TxNotification::AuthorizationTimeout);
            return;
        }
        
        if (localAuth) {
            AO_DBG_DEBUG("Offline transaction process (%s), locally authorized", tx->getIdTag());
            tx->setAuthorized();
            tx->commit();

            updateTxNotification(TxNotification::Authorized);
            return;
        }

        if (model.getReservationService()) {
            auto reservation = model.getReservationService()->getReservation(
                        connectorId,
                        tx->getIdTag());
            if (reservation && !reservation->matches(
                        tx->getIdTag())) {
                //reservation found for connector but does not match idTag or parentIdTag
                AO_DBG_INFO("connector %u reserved (offline) - abort transaction", connectorId);
                tx->setInactive();
                tx->commit();
                updateTxNotification(TxNotification::ReservationConflict);
                return;
            }
        }

        if (allowOfflineTxForUnknownId && (bool) *allowOfflineTxForUnknownId) {
            AO_DBG_DEBUG("Offline transaction process (%s), allow unknown ID", tx->getIdTag());
            tx->setAuthorized();
            tx->commit();
            updateTxNotification(TxNotification::Authorized);
            return;
        }
        
        AO_DBG_DEBUG("Abort transaction process (%s): timeout", tx->getIdTag());
        tx->setInactive();
        tx->commit();
        updateTxNotification(TxNotification::AuthorizationTimeout);
        return; //offline tx disabled
    });
    context.initiateRequest(std::move(authorize));

    return transaction;
}

std::shared_ptr<Transaction> Connector::beginTransaction_authorized(const char *idTag, const char *parentIdTag) {
    
    if (transaction) {
        AO_DBG_WARN("tx process still running. Please call endTransaction(...) before");
        return nullptr;
    }

    transaction = allocateTransaction();

    if (!transaction) {
        AO_DBG_ERR("could not allocate Tx");
        return nullptr;
    }

    if (!idTag || *idTag == '\0') {
        //input string is empty
        transaction->setIdTag("");
    } else {
        transaction->setIdTag(idTag);
    }

    transaction->setBeginTimestamp(model.getClock().now());
    
    AO_DBG_DEBUG("Begin transaction process (%s), already authorized", idTag != nullptr ? idTag : "");

    transaction->setAuthorized();
    
    if (model.getReservationService()) {
        if (auto reservation = model.getReservationService()->getReservation(connectorId, idTag, parentIdTag)) {
            if (reservation->matches(idTag, parentIdTag)) {
                transaction->setReservationId(reservation->getReservationId());
            }
        }
    }

    transaction->commit();

    return transaction;
}

void Connector::endTransaction(const char *idTag, const char *reason) {

    if (!transaction || !transaction->isActive()) {
        //transaction already ended / not active anymore
        return;
    }

    if (idTag && *idTag != '\0' && !strcmp(idTag, transaction->getIdTag())) {
        //given stop idTag differs from start idTag. Make Authorization check first

        AO_DBG_WARN("authorization status of stop idTag not checked automatically yet");
        (void)0;
    }

    AO_DBG_DEBUG("End session started by idTag %s",
                            transaction->getIdTag());
    
    if (idTag && *idTag != '\0') {
        transaction->setStopIdTag(idTag);
    }
    
    if (reason) {
        transaction->setStopReason(reason);
    }
    transaction->setInactive();
    transaction->commit();
}

std::shared_ptr<Transaction>& Connector::getTransaction() {
    return transaction;
}

bool Connector::isOperative() {
    if (isFaulted()) {
        return false;
    }

    if (!trackLoopExecute) {
        return false;
    }

    //check for running transaction(s) - if yes then the connector is always operative
    if (connectorId == 0) {
        for (unsigned int cId = 1; cId < model.getNumConnectors(); cId++) {
            if (model.getConnector(cId)->getTransaction() && model.getConnector(cId)->getTransaction()->isRunning()) {
                return true;
            }
        }
    } else {
        if (transaction && transaction->isRunning()) {
            return true;
        }
    }

    return availabilityVolatile && *availability;
}

void Connector::setAvailability(bool available) {
    *availability = available;
    configuration_save();
}

void Connector::setAvailabilityVolatile(bool available) {
    availabilityVolatile = available;
}

void Connector::setConnectorPluggedInput(std::function<bool()> connectorPlugged) {
    this->connectorPluggedInput = connectorPlugged;
}

void Connector::setEvReadyInput(std::function<bool()> evRequestsEnergy) {
    this->evReadyInput = evRequestsEnergy;
}

void Connector::setEvseReadyInput(std::function<bool()> connectorEnergized) {
    this->evseReadyInput = connectorEnergized;
}

void Connector::addErrorCodeInput(std::function<const char*()> connectorErrorCode) {
    addErrorDataInput([connectorErrorCode] () -> ErrorData {
        return ErrorData(connectorErrorCode());
    });
}

void Connector::addErrorDataInput(std::function<ErrorData ()> errorDataInput) {
    this->errorDataInputs.push_back(errorDataInput);
    this->trackErrorDataInputs.push_back(false);
}

void Connector::setOnUnlockConnector(std::function<PollResult<bool>()> unlockConnector) {
    this->onUnlockConnector = unlockConnector;
}

std::function<PollResult<bool>()> Connector::getOnUnlockConnector() {
    return this->onUnlockConnector;
}

void Connector::setStartTxReadyInput(std::function<bool()> startTxReady) {
    this->startTxReadyInput = startTxReady;
}

void Connector::setStopTxReadyInput(std::function<bool()> stopTxReady) {
    this->stopTxReadyInput = stopTxReady;
}

void Connector::setOccupiedInput(std::function<bool()> occupied) {
    this->occupiedInput = occupied;
}

void Connector::setTxNotificationOutput(std::function<void(Transaction*, TxNotification)> txNotificationOutput) {
    this->txNotificationOutput = txNotificationOutput;
}

void Connector::updateTxNotification(TxNotification event) {
    if (txNotificationOutput) {
        txNotificationOutput(transaction.get(), event);
    }
}
