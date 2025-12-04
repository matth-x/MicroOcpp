// matth-x/MicroOcpp
// Copyright Matthias Akstaller 2019 - 2024
// MIT License

#include <MicroOcpp/Model/ConnectorBase/Connector.h>

#include <MicroOcpp/Core/Context.h>
#include <MicroOcpp/Model/Model.h>
#include <MicroOcpp/Core/Request.h>
#include <MicroOcpp/Model/Transactions/TransactionStore.h>
#include <MicroOcpp/Core/Configuration.h>

#include <MicroOcpp/Operations/Authorize.h>
#include <MicroOcpp/Operations/StatusNotification.h>
#include <MicroOcpp/Operations/StartTransaction.h>
#include <MicroOcpp/Operations/StopTransaction.h>
#include <MicroOcpp/Operations/CiStrings.h>

#include <MicroOcpp/Debug.h>

#include <MicroOcpp/Model/Metering/MeteringService.h>
#include <MicroOcpp/Model/Reservation/ReservationService.h>
#include <MicroOcpp/Model/Authorization/AuthorizationService.h>
#include <MicroOcpp/Model/ConnectorBase/EvseId.h>
#include <MicroOcpp/Model/Transactions/TransactionService.h>

#include <MicroOcpp/Core/Request.h>
#include <MicroOcpp/Core/Connection.h>

#ifndef MO_TX_CLEAN_ABORTED
#define MO_TX_CLEAN_ABORTED 1
#endif

using namespace MicroOcpp;

Connector::Connector(Context& context, std::shared_ptr<FilesystemAdapter> filesystem, unsigned int connectorId)
        : MemoryManaged("v16.ConnectorBase.Connector"), context(context), model(context.getModel()), filesystem(filesystem), connectorId(connectorId),
          errorDataInputs(makeVector<std::function<ErrorData ()>>(getMemoryTag())), trackErrorDataInputs(makeVector<bool>(getMemoryTag())) {

    context.getRequestQueue().addSendQueue(this); //register at RequestQueue as Request emitter

    snprintf(availabilityBoolKey, sizeof(availabilityBoolKey), MO_CONFIG_EXT_PREFIX "AVAIL_CONN_%d", connectorId);
    availabilityBool = declareConfiguration<bool>(availabilityBoolKey, true, MO_KEYVALUE_FN, false, false, false);

#if MO_ENABLE_CONNECTOR_LOCK
    declareConfiguration<bool>("UnlockConnectorOnEVSideDisconnect", true); //read-write
#else
    declareConfiguration<bool>("UnlockConnectorOnEVSideDisconnect", false, CONFIGURATION_VOLATILE, true); //read-only because there is no connector lock
#endif //MO_ENABLE_CONNECTOR_LOCK

    connectionTimeOutInt = declareConfiguration<int>("ConnectionTimeOut", 30);
    registerConfigurationValidator("ConnectionTimeOut", VALIDATE_UNSIGNED_INT);
    minimumStatusDurationInt = declareConfiguration<int>("MinimumStatusDuration", 0);
    registerConfigurationValidator("MinimumStatusDuration", VALIDATE_UNSIGNED_INT);
    stopTransactionOnInvalidIdBool = declareConfiguration<bool>("StopTransactionOnInvalidId", true);
    stopTransactionOnEVSideDisconnectBool = declareConfiguration<bool>("StopTransactionOnEVSideDisconnect", true);
    localPreAuthorizeBool = declareConfiguration<bool>("LocalPreAuthorize", false);
    localAuthorizeOfflineBool = declareConfiguration<bool>("LocalAuthorizeOffline", true);
    allowOfflineTxForUnknownIdBool = MicroOcpp::declareConfiguration<bool>("AllowOfflineTxForUnknownId", false);

    //if the EVSE goes offline, can it continue to charge without sending StartTx / StopTx to the server when going online again?
    silentOfflineTransactionsBool = declareConfiguration<bool>(MO_CONFIG_EXT_PREFIX "SilentOfflineTransactions", false);

    //how long the EVSE tries the Authorize request before it enters offline mode
    authorizationTimeoutInt = MicroOcpp::declareConfiguration<int>(MO_CONFIG_EXT_PREFIX "AuthorizationTimeout", 20);
    registerConfigurationValidator(MO_CONFIG_EXT_PREFIX "AuthorizationTimeout", VALIDATE_UNSIGNED_INT);

    //FreeVend mode
    freeVendActiveBool = declareConfiguration<bool>(MO_CONFIG_EXT_PREFIX "FreeVendActive", false);
    freeVendIdTagString = declareConfiguration<const char*>(MO_CONFIG_EXT_PREFIX "FreeVendIdTag", "");

    txStartOnPowerPathClosedBool = declareConfiguration<bool>(MO_CONFIG_EXT_PREFIX "TxStartOnPowerPathClosed", false);

    transactionMessageAttemptsInt = declareConfiguration<int>("TransactionMessageAttempts", 3);
    registerConfigurationValidator("TransactionMessageAttempts", VALIDATE_UNSIGNED_INT);
    transactionMessageRetryIntervalInt = declareConfiguration<int>("TransactionMessageRetryInterval", 60);
    registerConfigurationValidator("TransactionMessageRetryInterval", VALIDATE_UNSIGNED_INT);

    if (!availabilityBool) {
        MO_DBG_ERR("Cannot declare availabilityBool");
    }

    char txFnamePrefix [30];
    snprintf(txFnamePrefix, sizeof(txFnamePrefix), "tx-%u-", connectorId);
    size_t txFnamePrefixLen = strlen(txFnamePrefix);

    unsigned int txNrPivot = std::numeric_limits<unsigned int>::max();

    if (filesystem) {
        filesystem->ftw_root([this, txFnamePrefix, txFnamePrefixLen, &txNrPivot] (const char *fname) {
            if (!strncmp(fname, txFnamePrefix, txFnamePrefixLen)) {
                unsigned int parsedTxNr = 0;
                for (size_t i = txFnamePrefixLen; fname[i] >= '0' && fname[i] <= '9'; i++) {
                    parsedTxNr *= 10;
                    parsedTxNr += fname[i] - '0';
                }

                if (txNrPivot == std::numeric_limits<unsigned int>::max()) {
                    txNrPivot = parsedTxNr;
                    txNrBegin = parsedTxNr;
                    txNrEnd = (parsedTxNr + 1) % MAX_TX_CNT;
                    return 0;
                }

                if ((parsedTxNr + MAX_TX_CNT - txNrPivot) % MAX_TX_CNT < MAX_TX_CNT / 2) {
                    //parsedTxNr is after pivot point
                    if ((parsedTxNr + 1 + MAX_TX_CNT - txNrPivot) % MAX_TX_CNT > (txNrEnd + MAX_TX_CNT - txNrPivot) % MAX_TX_CNT) {
                        txNrEnd = (parsedTxNr + 1) % MAX_TX_CNT;
                    }
                } else if ((txNrPivot + MAX_TX_CNT - parsedTxNr) % MAX_TX_CNT < MAX_TX_CNT / 2) {
                    //parsedTxNr is before pivot point
                    if ((txNrPivot + MAX_TX_CNT - parsedTxNr) % MAX_TX_CNT > (txNrPivot + MAX_TX_CNT - txNrBegin) % MAX_TX_CNT) {
                        txNrBegin = parsedTxNr;
                    }
                }

                MO_DBG_DEBUG("found %s%u.jsn - Internal range from %u to %u (exclusive)", txFnamePrefix, parsedTxNr, txNrBegin, txNrEnd);
            }
            return 0;
        });
    }

    MO_DBG_DEBUG("found %u transactions for connector %u. Internal range from %u to %u (exclusive)", (txNrEnd + MAX_TX_CNT - txNrBegin) % MAX_TX_CNT, connectorId, txNrBegin, txNrEnd);
    txNrFront = txNrBegin;

    if (model.getTransactionStore()) {
        unsigned int txNrLatest = (txNrEnd + MAX_TX_CNT - 1) % MAX_TX_CNT; //txNr of the most recent tx on flash
        transaction = model.getTransactionStore()->getTransaction(connectorId, txNrLatest); //returns nullptr if txNrLatest does not exist on flash
    } else {
        MO_DBG_ERR("must initialize TxStore before Connector");
    }
}

Connector::~Connector() {
    if (availabilityBool->getKey() == availabilityBoolKey) {
        availabilityBool->setKey(nullptr);
    }
}

ChargePointStatus Connector::getStatus() {

    ChargePointStatus res = ChargePointStatus_UNDEFINED;

    /*
    * Handle special case: This is the Connector for the whole CP (i.e. connectorId=0) --> only states Available, Unavailable, Faulted are possible
    */
    if (connectorId == 0) {
        if (isFaulted()) {
            res = ChargePointStatus_Faulted;
        } else if (!isOperative()) {
            res = ChargePointStatus_Unavailable;
        } else {
            res = ChargePointStatus_Available;
        }
        return res;
    }

    if (isFaulted()) {
        res = ChargePointStatus_Faulted;
    } else if (!isOperative()) {
        res = ChargePointStatus_Unavailable;
    } else if (transaction && transaction->isRunning()) {
        //Transaction is currently running
        if (connectorPluggedInput && !connectorPluggedInput()) { //special case when StopTransactionOnEVSideDisconnect is false
            res = ChargePointStatus_SuspendedEV;
        } else if (!ocppPermitsCharge() ||
                (evseReadyInput && !evseReadyInput())) { 
            res = ChargePointStatus_SuspendedEVSE;
        } else if (evReadyInput && !evReadyInput()) {
            res = ChargePointStatus_SuspendedEV;
        } else {
            res = ChargePointStatus_Charging;
        }
    }
    #if MO_ENABLE_RESERVATION
    else if (model.getReservationService() && model.getReservationService()->getReservation(connectorId)) {
        res = ChargePointStatus_Reserved;
    }
    #endif 
    else if ((!transaction) &&                                           //no transaction process occupying the connector
               (!connectorPluggedInput || !connectorPluggedInput()) &&   //no vehicle plugged
               (!occupiedInput || !occupiedInput())) {                       //occupied override clear
        res = ChargePointStatus_Available;
    } else {
        /*
         * Either in Preparing or Finishing state. Only way to know is from previous state
         */
        const auto previous = currentStatus;
        if (previous == ChargePointStatus_Finishing ||
                previous == ChargePointStatus_Charging ||
                previous == ChargePointStatus_SuspendedEV ||
                previous == ChargePointStatus_SuspendedEVSE ||
                (transaction && transaction->getStartSync().isRequested())) { //transaction process still occupying the connector
            res = ChargePointStatus_Finishing;
        } else {
            res = ChargePointStatus_Preparing;
        }
    }

#if MO_ENABLE_V201
    if (model.getVersion().major == 2) {
        //OCPP 2.0.1: map v1.6 status onto v2.0.1
        if (res == ChargePointStatus_Preparing ||
                res == ChargePointStatus_Charging ||
                res == ChargePointStatus_SuspendedEV ||
                res == ChargePointStatus_SuspendedEVSE ||
                res == ChargePointStatus_Finishing) {
            res = ChargePointStatus_Occupied;
        }
    }
#endif

    if (res == ChargePointStatus_UNDEFINED) {
        MO_DBG_DEBUG("status undefined");
        return ChargePointStatus_Faulted; //internal error
    }

    return res;
}

bool Connector::ocppPermitsCharge() {
    if (connectorId == 0) {
        MO_DBG_WARN("not supported for connectorId == 0");
        return false;
    }

    bool suspendDeAuthorizedIdTag = transaction && transaction->isIdTagDeauthorized(); //if idTag status is "DeAuthorized" and if charging should stop
    
    //check special case for DeAuthorized idTags: FreeVend mode
    if (suspendDeAuthorizedIdTag && freeVendActiveBool && freeVendActiveBool->getBool()) {
        suspendDeAuthorizedIdTag = false;
    }

    // check charge permission depending on TxStartPoint
    if (txStartOnPowerPathClosedBool && txStartOnPowerPathClosedBool->getBool()) {
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

void Connector::loop() {

    if (!trackLoopExecute) {
        trackLoopExecute = true;
        if (connectorPluggedInput) {
            freeVendTrackPlugged = connectorPluggedInput();
        }
    }

    if (transaction && ((transaction->isAborted() && MO_TX_CLEAN_ABORTED) || (transaction->isSilent() && transaction->getStopSync().isRequested()))) {
        //If the transaction is aborted (invalidated before started) or is silent and has stopped. Delete all artifacts from flash
        //This is an optimization. The memory management will attempt to remove those files again later
        bool removed = true;
        if (auto mService = model.getMeteringService()) {
            mService->abortTxMeterData(connectorId);
            removed &= mService->removeTxMeterData(connectorId, transaction->getTxNr());
        }

        if (removed) {
            removed &= model.getTransactionStore()->remove(connectorId, transaction->getTxNr());
        }

        if (removed) {
            if (txNrFront == txNrEnd) {
                txNrFront = transaction->getTxNr();
            }
            txNrEnd = transaction->getTxNr(); //roll back creation of last tx entry
        }

        MO_DBG_DEBUG("collect aborted or silent transaction %u-%u %s", connectorId, transaction->getTxNr(), removed ? "" : "failure");
        MO_DBG_VERBOSE("txNrBegin=%u, txNrFront=%u, txNrEnd=%u", txNrBegin, txNrFront, txNrEnd);
        transaction = nullptr;
    }

    if (transaction && transaction->isAborted()) {
        MO_DBG_DEBUG("collect aborted transaction %u-%u", connectorId, transaction->getTxNr());
        MO_DBG_VERBOSE("txNrBegin=%u, txNrFront=%u, txNrEnd=%u", txNrBegin, txNrFront, txNrEnd);
        transaction = nullptr;
    }

    if (transaction && transaction->getStopSync().isRequested()) {
        MO_DBG_DEBUG("collect obsolete transaction %u-%u", connectorId, transaction->getTxNr());
        MO_DBG_VERBOSE("txNrBegin=%u, txNrFront=%u, txNrEnd=%u", txNrBegin, txNrFront, txNrEnd);
        transaction = nullptr;
    }

    if (transaction) { //begin exclusively transaction-related operations
            
        if (connectorPluggedInput) {
            if (transaction->isRunning() && transaction->isActive() && !connectorPluggedInput()) {
                if (!stopTransactionOnEVSideDisconnectBool || stopTransactionOnEVSideDisconnectBool->getBool()) {
                    MO_DBG_DEBUG("Stop Tx due to EV disconnect");
                    transaction->setStopReason("EVDisconnected");
                    transaction->setInactive();
                    transaction->commit();
                }
            }

            if (transaction->isActive() &&
                    !transaction->getStartSync().isRequested() &&
                    transaction->getBeginTimestamp() > MIN_TIME &&
                    connectionTimeOutInt && connectionTimeOutInt->getInt() > 0 &&
                    !connectorPluggedInput() &&
                    model.getClock().now() - transaction->getBeginTimestamp() > connectionTimeOutInt->getInt()) {

                MO_DBG_INFO("Session mngt: timeout");
                transaction->setInactive();
                transaction->commit();

                updateTxNotification(TxNotification_ConnectionTimeout);
            }
        }

        if (transaction->isActive() &&
                transaction->isIdTagDeauthorized() && ( //transaction has been deAuthorized
                    !transaction->isRunning() ||        //if transaction hasn't started yet, always end
                    !stopTransactionOnInvalidIdBool || stopTransactionOnInvalidIdBool->getBool())) { //if transaction is running, behavior depends on StopTransactionOnInvalidId
            
            MO_DBG_DEBUG("DeAuthorize session");
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
                    (!txStartOnPowerPathClosedBool || !txStartOnPowerPathClosedBool->getBool() || !evReadyInput || evReadyInput()) && //if applicable, postpone tx start point to PowerPathClosed
                    (!startTxReadyInput || startTxReadyInput())) { //if defined, user Input for allowing StartTx must be true
                //start Transaction

                MO_DBG_INFO("Session mngt: trigger StartTransaction");

                auto meteringService = model.getMeteringService();
                if (transaction->getMeterStart() < 0 && meteringService) {
                    auto meterStart = meteringService->readTxEnergyMeter(transaction->getConnectorId(), ReadingContext_TransactionBegin);
                    if (meterStart && *meterStart) {
                        transaction->setMeterStart(meterStart->toInteger());
                    } else {
                        MO_DBG_ERR("MeterStart undefined");
                    }
                }

                if (transaction->getStartTimestamp() <= MIN_TIME) {
                    transaction->setStartTimestamp(model.getClock().now());
                    transaction->setStartBootNr(model.getBootNr());
                }

                transaction->getStartSync().setRequested();
                transaction->getStartSync().setOpNr(context.getRequestQueue().getNextOpNr());

                if (transaction->isSilent()) {
                    MO_DBG_INFO("silent Transaction: omit StartTx");
                    transaction->getStartSync().confirm();
                } else {
                    //normal transaction, record txMeterData
                    if (model.getMeteringService()) {
                        model.getMeteringService()->beginTxMeterData(transaction.get());
                    }
                }

                transaction->commit();

                updateTxNotification(TxNotification_StartTx);

                //fetchFrontRequest will create the StartTransaction and pass it to the message sender
                return;
            }
        } else {
            //stop tx?

            if (!transaction->isActive() &&
                    (!stopTxReadyInput || stopTxReadyInput())) {
                //stop transaction

                MO_DBG_INFO("Session mngt: trigger StopTransaction");
                
                auto meteringService = model.getMeteringService();
                if (transaction->getMeterStop() < 0 && meteringService) {
                    auto meterStop = meteringService->readTxEnergyMeter(transaction->getConnectorId(), ReadingContext_TransactionEnd);
                    if (meterStop && *meterStop) {
                        transaction->setMeterStop(meterStop->toInteger());
                    } else {
                        MO_DBG_ERR("MeterStop undefined");
                    }
                }

                if (transaction->getStopTimestamp() <= MIN_TIME) {
                    transaction->setStopTimestamp(model.getClock().now());
                    transaction->setStopBootNr(model.getBootNr());
                }

                transaction->getStopSync().setRequested();
                transaction->getStopSync().setOpNr(context.getRequestQueue().getNextOpNr());

                if (transaction->isSilent()) {
                    MO_DBG_INFO("silent Transaction: omit StopTx");
                    transaction->getStopSync().confirm();
                } else {
                    //normal transaction, record txMeterData
                    if (model.getMeteringService()) {
                        model.getMeteringService()->endTxMeterData(transaction.get());
                    }
                }

                transaction->commit();

                updateTxNotification(TxNotification_StopTx);

                //fetchFrontRequest will create the StopTransaction and pass it to the message sender
                return;
            }
        }
    } //end transaction-related operations

    //handle FreeVend mode
    if (freeVendActiveBool && freeVendActiveBool->getBool() && connectorPluggedInput) {
        if (!freeVendTrackPlugged && connectorPluggedInput() && !transaction) {
            const char *idTag = freeVendIdTagString ? freeVendIdTagString->getString() : "";
            if (!idTag || *idTag == '\0') {
                idTag = "A0000000";
            }
            MO_DBG_INFO("begin FreeVend Tx using idTag %s", idTag);
            beginTransaction_authorized(idTag);
            
            if (!transaction) {
                MO_DBG_ERR("could not begin FreeVend Tx");
            }
        }

        freeVendTrackPlugged = connectorPluggedInput();
    }

    ErrorData errorData {nullptr};
    errorData.severity = 0;
    int errorDataIndex = -1;

    if (model.getVersion().major == 1 && model.getClock().now() >= MIN_TIME) {
        //OCPP 1.6: use StatusNotification to send error codes

        if (reportedErrorIndex >= 0) {
            auto error = errorDataInputs[reportedErrorIndex].operator()();
            if (error.isError) {
                errorData = error;
                errorDataIndex = reportedErrorIndex;
            }
        }

        for (auto i = std::min(errorDataInputs.size(), trackErrorDataInputs.size()); i >= 1; i--) {
            auto index = i - 1;
            ErrorData error {nullptr};
            if ((int)index != errorDataIndex) {
                error = errorDataInputs[index].operator()();
            } else {
                error = errorData;
            }
            if (error.isError && !trackErrorDataInputs[index] && error.severity >= errorData.severity) {
                //new error
                errorData = error;
                errorDataIndex = index;
            } else if (error.isError && error.severity > errorData.severity) {
                errorData = error;
                errorDataIndex = index;
            } else if (!error.isError && trackErrorDataInputs[index]) {
                //reset error
                trackErrorDataInputs[index] = false;
            }
        }

        if (errorDataIndex != reportedErrorIndex) {
            if (errorDataIndex >= 0 || MO_REPORT_NOERROR) {
                reportedStatus = ChargePointStatus_UNDEFINED; //trigger sending currentStatus again with code NoError
            } else {
                reportedErrorIndex = -1;
            }
        }
    } //if (model.getVersion().major == 1)

    auto status = getStatus();

    if (status != currentStatus) {
        MO_DBG_DEBUG("Status changed %s -> %s %s",
                currentStatus == ChargePointStatus_UNDEFINED ? "" : cstrFromOcppEveState(currentStatus),
                cstrFromOcppEveState(status),
                minimumStatusDurationInt->getInt() ? " (will report delayed)" : "");
        currentStatus = status;
        t_statusTransition = mocpp_tick_ms();
    }

    if (reportedStatus != currentStatus &&
            model.getClock().now() >= MIN_TIME &&
            (minimumStatusDurationInt->getInt() <= 0 || //MinimumStatusDuration disabled
            mocpp_tick_ms() - t_statusTransition >= ((unsigned long) minimumStatusDurationInt->getInt()) * 1000UL)) {
        reportedStatus = currentStatus;
        reportedErrorIndex = errorDataIndex;
        if (errorDataIndex >= 0) {
            trackErrorDataInputs[errorDataIndex] = true;
        }
        Timestamp reportedTimestamp = model.getClock().now();
        reportedTimestamp -= (mocpp_tick_ms() - t_statusTransition) / 1000UL;

        auto statusNotification =
            #if MO_ENABLE_V201
            model.getVersion().major == 2 ?
                makeRequest(
                    new Ocpp201::StatusNotification(connectorId, reportedStatus, reportedTimestamp)) :
            #endif //MO_ENABLE_V201
                makeRequest(
                    new Ocpp16::StatusNotification(connectorId, reportedStatus, reportedTimestamp, errorData));

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
    if (reportedErrorIndex >= 0) {
        auto error = errorDataInputs[reportedErrorIndex].operator()();
        if (error.isError && error.errorCode) {
            return error.errorCode;
        }
    }
    return nullptr;
}

std::shared_ptr<Transaction> Connector::allocateTransaction() {

    std::shared_ptr<Transaction> tx;

    //clean possible aborted tx
    unsigned int txr = txNrEnd;
    unsigned int txSize = (txNrEnd + MAX_TX_CNT - txNrBegin) % MAX_TX_CNT;
    for (unsigned int i = 0; i < txSize; i++) {
        txr = (txr + MAX_TX_CNT - 1) % MAX_TX_CNT; //decrement by 1

        auto tx = model.getTransactionStore()->getTransaction(connectorId, txr);
        //check if dangling silent tx, aborted tx, or corrupted entry (tx == null)
        if (!tx || tx->isSilent() || (tx->isAborted() && MO_TX_CLEAN_ABORTED)) {
            //yes, remove
            bool removed = true;
            if (auto mService = model.getMeteringService()) {
                removed &= mService->removeTxMeterData(connectorId, txr);
            }
            if (removed) {
                removed &= model.getTransactionStore()->remove(connectorId, txr);
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

    txSize = (txNrEnd + MAX_TX_CNT - txNrBegin) % MAX_TX_CNT; //refresh after cleaning txs

    //try to create new transaction
    if (txSize < MO_TXRECORD_SIZE) {
        tx = model.getTransactionStore()->createTransaction(connectorId, txNrEnd);
    }

    if (!tx) {
        //could not create transaction - now, try to replace tx history entry

        unsigned int txl = txNrBegin;
        txSize = (txNrEnd + MAX_TX_CNT - txNrBegin) % MAX_TX_CNT;

        for (unsigned int i = 0; i < txSize; i++) {

            if (tx) {
                //success, finished here
                break;
            }

            //no transaction allocated, delete history entry to make space

            auto txhist = model.getTransactionStore()->getTransaction(connectorId, txl);
            //oldest entry, now check if it's history and can be removed or corrupted entry
            if (!txhist || txhist->isCompleted() || txhist->isAborted() || (txhist->isSilent() && txhist->getStopSync().isRequested())) {
                //yes, remove
                bool removed = true;
                if (auto mService = model.getMeteringService()) {
                    removed &= mService->removeTxMeterData(connectorId, txl);
                }
                if (removed) {
                    removed &= model.getTransactionStore()->remove(connectorId, txl);
                }
                if (removed) {
                    txNrBegin = (txl + 1) % MAX_TX_CNT;
                    if (txNrFront == txl) {
                        txNrFront = txNrBegin;
                    }
                    MO_DBG_DEBUG("deleted tx history entry for new transaction");
                    MO_DBG_VERBOSE("txNrBegin=%u, txNrFront=%u, txNrEnd=%u", txNrBegin, txNrFront, txNrEnd);

                    tx = model.getTransactionStore()->createTransaction(connectorId, txNrEnd);
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
            txl %= MAX_TX_CNT;
        }
    }

    if (!tx) {
        //couldn't create normal transaction -> check if to start charging without real transaction
        if (silentOfflineTransactionsBool && silentOfflineTransactionsBool->getBool()) {
            //try to handle charging session without sending StartTx or StopTx to the server
            tx = model.getTransactionStore()->createTransaction(connectorId, txNrEnd, true);

            if (tx) {
                MO_DBG_DEBUG("created silent transaction");
            }
        }
    }

    if (tx) {
        //clean meter data which could still be here from a rolled-back transaction
        if (auto mService = model.getMeteringService()) {
            if (!mService->removeTxMeterData(connectorId, tx->getTxNr())) {
                MO_DBG_ERR("memory corruption");
            }
        }
    }

    if (tx) {
        txNrEnd = (txNrEnd + 1) % MAX_TX_CNT;
        MO_DBG_DEBUG("advance txNrEnd %u-%u", connectorId, txNrEnd);
        MO_DBG_VERBOSE("txNrBegin=%u, txNrFront=%u, txNrEnd=%u", txNrBegin, txNrFront, txNrEnd);
    }

    return tx;
}

std::shared_ptr<Transaction> Connector::beginTransaction(const char *idTag) {

    if (transaction) {
        MO_DBG_WARN("tx process still running. Please call endTransaction(...) before");
        return nullptr;
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
        if (localAuth && localAuth->getExpiryDate() && *localAuth->getExpiryDate() < model.getClock().now()) {
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
                connectorId,
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
                MO_DBG_INFO("connector %u reserved - abort transaction", connectorId);
                updateTxNotification(TxNotification_ReservationConflict);
                return nullptr;
            } else {
                //parentIdTag unkown but local authorization failed in any case
                MO_DBG_INFO("connector %u reserved - no local auth", connectorId);
                localAuthFound = false;
            }
        }
    }
    #endif //MO_ENABLE_RESERVATION

    transaction = allocateTransaction();

    if (!transaction) {
        MO_DBG_ERR("could not allocate Tx");
        return nullptr;
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

    transaction->setBeginTimestamp(model.getClock().now());

    //check for local preauthorization
    if (localAuthFound && localPreAuthorizeBool && localPreAuthorizeBool->getBool()) {
        MO_DBG_DEBUG("Begin transaction process (%s), preauthorized locally", idTag != nullptr ? idTag : "");

        if (reservationId >= 0) {
            transaction->setReservationId(reservationId);
        }
        transaction->setAuthorized();

        updateTxNotification(TxNotification_Authorized);

        //TC_008_1_CS & TC_008_2_CS / The Charge Point does NOT send a Authorize.req
        if (localAuthorizeOfflineBool && localAuthorizeOfflineBool->getBool()) {
            transaction->commit();
            return transaction;
        }
    }

    transaction->commit();

    auto authorize = makeRequest(new Ocpp16::Authorize(context.getModel(), idTag));
    authorize->setTimeout(authorizationTimeoutInt && authorizationTimeoutInt->getInt() > 0 ? authorizationTimeoutInt->getInt() * 1000UL : 20UL * 1000UL);

    if (!context.getConnection().isConnected()) {
        //WebSockt unconnected. Enter offline mode immediately
        authorize->setTimeout(1);
    }

    auto tx = transaction;
    authorize->setOnReceiveConfListener([this, tx] (JsonObject response) {
        JsonObject idTagInfo = response["idTagInfo"];

        if (strcmp("Accepted", idTagInfo["status"] | "UNDEFINED")) {
            //Authorization rejected, abort transaction
            MO_DBG_DEBUG("Authorize rejected (%s), abort tx process", tx->getIdTag());
            tx->setIdTagDeauthorized();
            tx->commit();
            updateTxNotification(TxNotification_AuthorizationRejected);
            return;
        }

        #if MO_ENABLE_RESERVATION
        if (model.getReservationService()) {
            auto reservation = model.getReservationService()->getReservation(
                        connectorId,
                        tx->getIdTag(),
                        idTagInfo["parentIdTag"] | (const char*) nullptr);
            if (reservation) {
                //reservation found for connector
                if (reservation->matches(
                            tx->getIdTag(),
                            idTagInfo["parentIdTag"] | (const char*) nullptr)) {
                    MO_DBG_INFO("connector %u matches reservationId %i", connectorId, reservation->getReservationId());
                    tx->setReservationId(reservation->getReservationId());
                } else {
                    //reservation found for connector but does not match idTag or parentIdTag
                    MO_DBG_INFO("connector %u reserved - abort transaction", connectorId);
                    tx->setInactive();
                    tx->commit();
                    updateTxNotification(TxNotification_ReservationConflict);
                    return;
                }
            }
        }
        #endif //MO_ENABLE_RESERVATION

        if (idTagInfo.containsKey("parentIdTag")) {
            tx->setParentIdTag(idTagInfo["parentIdTag"] | "");
        }

        MO_DBG_DEBUG("Authorized transaction process (%s)", tx->getIdTag());
        tx->setAuthorized();
        tx->commit();

        updateTxNotification(TxNotification_Authorized);
    });

    //capture local auth and reservation check in for timeout handler
    authorize->setOnTimeoutListener([this, tx,
                offlineBlockedAuth, 
                offlineBlockedResv, 
                localAuthFound,
                reservationId] () {

        if (offlineBlockedAuth) {
            //local auth entry exists, but is expired -> avoid offline tx
            MO_DBG_DEBUG("Abort transaction process (%s), timeout, expired local auth", tx->getIdTag());
            tx->setInactive();
            tx->commit();
            updateTxNotification(TxNotification_AuthorizationTimeout);
            return;
        }

        if (offlineBlockedResv) {
            //reservation found for connector but does not match idTag or parentIdTag
            MO_DBG_INFO("connector %u reserved (offline) - abort transaction", connectorId);
            tx->setInactive();
            tx->commit();
            updateTxNotification(TxNotification_ReservationConflict);
            return;
        }

        if (localAuthFound && localAuthorizeOfflineBool && localAuthorizeOfflineBool->getBool()) {
            MO_DBG_DEBUG("Offline transaction process (%s), locally authorized", tx->getIdTag());
            if (reservationId >= 0) {
                tx->setReservationId(reservationId);
            }
            tx->setAuthorized();
            tx->commit();

            updateTxNotification(TxNotification_Authorized);
            return;
        }

        if (allowOfflineTxForUnknownIdBool && allowOfflineTxForUnknownIdBool->getBool()) {
            MO_DBG_DEBUG("Offline transaction process (%s), allow unknown ID", tx->getIdTag());
            if (reservationId >= 0) {
                tx->setReservationId(reservationId);
            }
            tx->setAuthorized();
            tx->commit();
            updateTxNotification(TxNotification_Authorized);
            return;
        }

        MO_DBG_DEBUG("Abort transaction process (%s): timeout", tx->getIdTag());
        tx->setInactive();
        tx->commit();
        updateTxNotification(TxNotification_AuthorizationTimeout);
        return; //offline tx disabled
    });
    context.initiateRequest(std::move(authorize));

    return transaction;
}

std::shared_ptr<Transaction> Connector::beginTransaction_authorized(const char *idTag, const char *parentIdTag) {
    
    if (transaction) {
        MO_DBG_WARN("tx process still running. Please call endTransaction(...) before");
        return nullptr;
    }

    transaction = allocateTransaction();

    if (!transaction) {
        MO_DBG_ERR("could not allocate Tx");
        return nullptr;
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

    transaction->setBeginTimestamp(model.getClock().now());
    
    MO_DBG_DEBUG("Begin transaction process (%s), already authorized", idTag != nullptr ? idTag : "");

    transaction->setAuthorized();

    #if MO_ENABLE_RESERVATION
    if (model.getReservationService()) {
        if (auto reservation = model.getReservationService()->getReservation(connectorId, idTag, parentIdTag)) {
            if (reservation->matches(idTag, parentIdTag)) {
                transaction->setReservationId(reservation->getReservationId());
            }
        }
    }
    #endif //MO_ENABLE_RESERVATION

    transaction->commit();

    return transaction;
}

void Connector::endTransaction(const char *idTag, const char *reason) {

    if (!transaction || !transaction->isActive()) {
        //transaction already ended / not active anymore
        return;
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

    #if MO_ENABLE_V201
    if (model.getVersion().major == 2 && model.getTransactionService()) {
        auto txService = model.getTransactionService();

        if (connectorId == 0) {
            for (unsigned int cId = 1; cId < model.getNumConnectors(); cId++) {
                if (txService->getEvse(cId)->getTransaction() &&
                        txService->getEvse(cId)->getTransaction()->started &&
                        !txService->getEvse(cId)->getTransaction()->stopped) {
                    return true;
                }
            }
        } else {
            if (txService->getEvse(connectorId)->getTransaction() &&
                    txService->getEvse(connectorId)->getTransaction()->started &&
                    !txService->getEvse(connectorId)->getTransaction()->stopped) {
                return true;
            }
        }
    }
    #endif //MO_ENABLE_V201

    return availabilityVolatile && availabilityBool->getBool();
}

void Connector::setAvailability(bool available) {
    availabilityBool->setBool(available);
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

#if MO_ENABLE_CONNECTOR_LOCK
void Connector::setOnUnlockConnector(std::function<UnlockConnectorResult()> unlockConnector) {
    this->onUnlockConnector = unlockConnector;
}

std::function<UnlockConnectorResult()> Connector::getOnUnlockConnector() {
    return this->onUnlockConnector;
}
#endif //MO_ENABLE_CONNECTOR_LOCK

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

unsigned int Connector::getFrontRequestOpNr() {

    /*
     * Advance front transaction?
     */

    unsigned int txSize = (txNrEnd + MAX_TX_CNT - txNrFront) % MAX_TX_CNT;

    if (transactionFront && txSize == 0) {
        //catch edge case where txBack has been rolled back and txFront was equal to txBack
        MO_DBG_DEBUG("collect front transaction %u-%u after tx rollback", connectorId, transactionFront->getTxNr());
        MO_DBG_VERBOSE("txNrBegin=%u, txNrFront=%u, txNrEnd=%u", txNrBegin, txNrFront, txNrEnd);
        transactionFront = nullptr;
    }

    for (unsigned int i = 0; i < txSize; i++) {

        if (!transactionFront) {
            transactionFront = model.getTransactionStore()->getTransaction(connectorId, txNrFront);

            #if MO_DBG_LEVEL >= MO_DL_VERBOSE
            if (transactionFront)
            {
                MO_DBG_VERBOSE("load front transaction %u-%u", connectorId, transactionFront->getTxNr());
            }
            #endif
        }

        if (transactionFront && (transactionFront->isAborted() || transactionFront->isCompleted() || transactionFront->isSilent())) {
            //advance front
            MO_DBG_DEBUG("collect front transaction %u-%u", connectorId, transactionFront->getTxNr());
            transactionFront = nullptr;
            txNrFront = (txNrFront + 1) % MAX_TX_CNT;
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

std::unique_ptr<Request> Connector::fetchFrontRequest() {

    if (transactionFront && !transactionFront->isSilent()) {
        if (transactionFront->getStartSync().isRequested() && !transactionFront->getStartSync().isConfirmed()) {
            //send StartTx?

            bool cancelStartTx = false;

            if (transactionFront->getStartTimestamp() < MIN_TIME &&
                    transactionFront->getStartBootNr() != model.getBootNr()) {
                //time not set, cannot be restored anymore -> invalid tx
                MO_DBG_ERR("cannot recover tx from previus run");

                cancelStartTx = true;
            }

            if ((int)transactionFront->getStartSync().getAttemptNr() >= transactionMessageAttemptsInt->getInt()) {
                MO_DBG_WARN("exceeded TransactionMessageAttempts. Discard transaction");

                cancelStartTx = true;
            }

            if (cancelStartTx) {
                transactionFront->setSilent();
                transactionFront->setInactive();
                transactionFront->commit();

                //clean up possible tx records
                if (auto mSerivce = model.getMeteringService()) {
                    mSerivce->removeTxMeterData(connectorId, transactionFront->getTxNr());
                }
                //next getFrontRequestOpNr() call will collect transactionFront
                return nullptr;
            }

            Timestamp nextAttempt = transactionFront->getStartSync().getAttemptTime() +
                                    transactionFront->getStartSync().getAttemptNr() * std::max(0, transactionMessageRetryIntervalInt->getInt());

            if (nextAttempt > model.getClock().now()) {
                return nullptr;
            }

            transactionFront->getStartSync().advanceAttemptNr();
            transactionFront->getStartSync().setAttemptTime(model.getClock().now());
            transactionFront->commit();

            auto startTx = makeRequest(new Ocpp16::StartTransaction(model, transactionFront));
            startTx->setOnReceiveConfListener([this] (JsonObject response) {
                //fetch authorization status from StartTransaction.conf() for user notification

                const char* idTagInfoStatus = response["idTagInfo"]["status"] | "_Undefined";
                if (strcmp(idTagInfoStatus, "Accepted")) {
                    updateTxNotification(TxNotification_DeAuthorized);
                }
            });
            auto transactionFront_capture = transactionFront;
            startTx->setOnAbortListener([this, transactionFront_capture] () {
                //shortcut to the attemptNr check above. Relevant if other operations block the queue while this StartTx is timing out
                if (transactionFront_capture && (int)transactionFront_capture->getStartSync().getAttemptNr() >= transactionMessageAttemptsInt->getInt()) {
                    MO_DBG_WARN("exceeded TransactionMessageAttempts. Discard transaction");

                    transactionFront_capture->setSilent();
                    transactionFront_capture->setInactive();
                    transactionFront_capture->commit();

                    //clean up possible tx records
                    if (auto mSerivce = model.getMeteringService()) {
                        mSerivce->removeTxMeterData(connectorId, transactionFront_capture->getTxNr());
                    }
                    //next getFrontRequestOpNr() call will collect transactionFront
                }
            });

            return startTx;
        }

        if (transactionFront->getStopSync().isRequested() && !transactionFront->getStopSync().isConfirmed()) {
            //send StopTx?

            if ((int)transactionFront->getStopSync().getAttemptNr() >= transactionMessageAttemptsInt->getInt()) {
                MO_DBG_WARN("exceeded TransactionMessageAttempts. Discard transaction");

                transactionFront->setSilent();

                //clean up possible tx records
                if (auto mSerivce = model.getMeteringService()) {
                    mSerivce->removeTxMeterData(connectorId, transactionFront->getTxNr());
                }
                //next getFrontRequestOpNr() call will collect transactionFront
                return nullptr;
            }

            Timestamp nextAttempt = transactionFront->getStopSync().getAttemptTime() +
                                    transactionFront->getStopSync().getAttemptNr() * std::max(0, transactionMessageRetryIntervalInt->getInt());

            if (nextAttempt > model.getClock().now()) {
                return nullptr;
            }

            transactionFront->getStopSync().advanceAttemptNr();
            transactionFront->getStopSync().setAttemptTime(model.getClock().now());
            transactionFront->commit();

            std::shared_ptr<TransactionMeterData> stopTxData;

            if (auto meteringService = model.getMeteringService()) {
                stopTxData = meteringService->getStopTxMeterData(transactionFront.get());
            }

            std::unique_ptr<Request> stopTx;

            if (stopTxData) {
                stopTx = makeRequest(new Ocpp16::StopTransaction(model, transactionFront, stopTxData->retrieveStopTxData()));
            } else {
                stopTx = makeRequest(new Ocpp16::StopTransaction(model, transactionFront));
            }
            auto transactionFront_capture = transactionFront;
            stopTx->setOnAbortListener([this, transactionFront_capture] () {
                //shortcut to the attemptNr check above. Relevant if other operations block the queue while this StopTx is timing out
                if ((int)transactionFront_capture->getStopSync().getAttemptNr() >= transactionMessageAttemptsInt->getInt()) {
                    MO_DBG_WARN("exceeded TransactionMessageAttempts. Discard transaction");

                    transactionFront_capture->setSilent();
                    transactionFront_capture->setInactive();
                    transactionFront_capture->commit();

                    //clean up possible tx records
                    if (auto mSerivce = model.getMeteringService()) {
                        mSerivce->removeTxMeterData(connectorId, transactionFront_capture->getTxNr());
                    }
                    //next getFrontRequestOpNr() call will collect transactionFront
                }
            });

            return stopTx;
        }
    }

    return nullptr;
}

bool Connector::triggerStatusNotification() {

    ErrorData errorData {nullptr};
    errorData.severity = 0;

    if (reportedErrorIndex >= 0) {
        errorData = errorDataInputs[reportedErrorIndex].operator()();
    } else {
        //find errorData with maximum severity
        for (auto i = errorDataInputs.size(); i >= 1; i--) {
            auto index = i - 1;
            ErrorData error = errorDataInputs[index].operator()();
            if (error.isError && error.severity >= errorData.severity) {
                errorData = error;
            }
        }
    }

    auto statusNotification = makeRequest(new Ocpp16::StatusNotification(
                connectorId,
                getStatus(),
                context.getModel().getClock().now(),
                errorData));

    statusNotification->setTimeout(60000);

    context.getRequestQueue().sendRequestPreBoot(std::move(statusNotification));

    return true;
}

unsigned int Connector::getTxNrBeginHistory() {
    return txNrBegin;
}

unsigned int Connector::getTxNrFront() {
    return txNrFront;
}

unsigned int Connector::getTxNrEnd() {
    return txNrEnd;
}
