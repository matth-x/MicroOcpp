// matth-x/MicroOcpp
// Copyright Matthias Akstaller 2019 - 2024
// MIT License

#include <MicroOcpp/Version.h>

#if MO_ENABLE_V201

#include <MicroOcpp/Model/Transactions/TransactionService.h>

#include <string.h>

#include <MicroOcpp/Core/Context.h>
#include <MicroOcpp/Core/Request.h>
#include <MicroOcpp/Model/Model.h>
#include <MicroOcpp/Model/Variables/VariableService.h>
#include <MicroOcpp/Operations/TransactionEvent.h>
#include <MicroOcpp/Operations/CustomOperation.h>
#include <MicroOcpp/Core/SimpleRequestFactory.h>
#include <MicroOcpp/Debug.h>

using namespace MicroOcpp;
using namespace MicroOcpp::Ocpp201;

TransactionService::Evse::Evse(Context& context, TransactionService& txService, unsigned int evseId) :
        context(context), txService(txService), evseId(evseId) {

}

std::unique_ptr<Ocpp201::Transaction> TransactionService::Evse::allocateTransaction() {
    auto tx =  std::unique_ptr<Ocpp201::Transaction>(new Ocpp201::Transaction());
    if (!tx) {
        // OOM
        return nullptr;
    }

    //simple clock-based hash
    int v = context.getModel().getClock().now() - Timestamp(2020,0,0,0,0,0);
    unsigned int h = v;
    h *= 749572633U;
    h %= 24593209U;
    for (size_t i = 0; i < sizeof(tx->transactionId) - 3; i += 2) {
        sprintf(tx->transactionId + i, "%02X", (uint8_t)h);
        h *= 749572633U;
        h %= 24593209U;
    }

    return tx;
}

void TransactionService::Evse::loop() {

    bool txStarted = false;
    bool txStopped = false;

    bool txUpdated = false;

    TransactionEventData::TriggerReason triggerReason; // only valid if txStarted || txStopped || txUpdated
    TransactionEventData::StopReason stoppedReason; // only valid if txStopped

    if (!txStarted && !txStopped && !txUpdated) {
        if (connectorPluggedInput && connectorPluggedInput()) {
            if (!transaction && txService.isTxStartPoint(TxStartStopPoint::EVConnected)) {
                transaction = allocateTransaction();
                txStarted = true;
            }

            if (transaction && !transaction->evConnected.triggered) {
                transaction->evConnected.triggered = true;
                txUpdated = true;
                triggerReason = TransactionEventData::TriggerReason::CablePluggedIn;
            }
        } else if (connectorPluggedInput && !connectorPluggedInput()) {
            if (transaction && transaction->evConnected.triggered) {
                if (!transaction->evConnected.untriggered) {
                    transaction->evConnected.untriggered = true;
                    txUpdated = true;
                    triggerReason = TransactionEventData::TriggerReason::EVCommunicationLost;
                }

                if (txService.isTxStopPoint(TxStartStopPoint::EVConnected)) {
                    txStopped = true;
                    stoppedReason = TransactionEventData::StopReason::EVDisconnected;
                }
            }
        }
    }

    if (!txStarted && !txStopped && !txUpdated) {
        if (evseReadyInput && evseReadyInput()) {
            if (!transaction && txService.isTxStartPoint(TxStartStopPoint::PowerPathClosed)) {
                transaction = allocateTransaction();
                txStarted = true;
            }

            if (transaction && !transaction->powerPathClosed.triggered) {
                transaction->powerPathClosed.triggered = true;
                txUpdated = true;
                triggerReason = TransactionEventData::TriggerReason::ChargingStateChanged;
            }
        } else if (evseReadyInput && !evseReadyInput()) {
            if (transaction && transaction->powerPathClosed.triggered) {
                if (!transaction->powerPathClosed.untriggered) {
                    transaction->powerPathClosed.untriggered = true;
                    txUpdated = true;
                    triggerReason = TransactionEventData::TriggerReason::ChargingStateChanged;
                }

                if (txService.isTxStopPoint(TxStartStopPoint::PowerPathClosed)) {
                    txStopped = true;
                    stoppedReason = TransactionEventData::StopReason::StoppedByEV;
                }
            }
        }
    }

    if (!txStarted && !txStopped && !txUpdated) {
        if (authorization.get()) {
            if (!transaction && txService.isTxStartPoint(TxStartStopPoint::Authorized)) {
                transaction = allocateTransaction();
                txStarted = true;
            }

            if (transaction && !transaction->authorized.triggered) {
                transaction->authorized.triggered = true;
                txUpdated = true;
                triggerReason = TransactionEventData::TriggerReason::Authorized;
            }
        } else {
            if (transaction && transaction->authorized.triggered) {
                if (!transaction->authorized.untriggered) {
                    transaction->authorized.untriggered = true;
                    txUpdated = true;
                    triggerReason = TransactionEventData::TriggerReason::StopAuthorized;
                }

                if (txService.isTxStopPoint(TxStartStopPoint::EVConnected)) {
                    txStopped = true;
                    stoppedReason = TransactionEventData::StopReason::Local;
                }
            }
        }
    }

    if (txStarted) {
       transaction->evConnected.triggered |= connectorPluggedInput && connectorPluggedInput();
       transaction->powerPathClosed.triggered |= evseReadyInput && evseReadyInput();
       transaction->authorized.triggered |= authorization.get() != nullptr;
    }

    TransactionEventData::ChargingState chargingState = TransactionEventData::ChargingState::Idle;
    if (transaction) {
        chargingState = TransactionEventData::ChargingState::Charging;

        if (connectorPluggedInput && !connectorPluggedInput()) {
            chargingState = TransactionEventData::ChargingState::Idle;
        } else if (!authorization.get()) {
            chargingState = TransactionEventData::ChargingState::EVConnected;
        } else if (evseReadyInput && !evseReadyInput()) { 
            chargingState = TransactionEventData::ChargingState::SuspendedEVSE;
        } else if (evReadyInput && !evReadyInput()) {
            chargingState = TransactionEventData::ChargingState::SuspendedEV;
        } else {
            chargingState = TransactionEventData::ChargingState::Charging;
        }
    }

    if (txStarted && txStopped) {
        MO_DBG_ERR("tx started and stopped");
        transaction.reset();
        return;
    } else if (txStarted || txStopped || txUpdated) {
        auto txEvent = std::make_shared<TransactionEventData>(transaction, transaction->seqNoCounter++);
        if (!txEvent) {
            // OOM
            transaction->active = false;
            return;
        }

        txEvent->eventType = txStarted ? TransactionEventData::Type::Started :
                             txStopped ? TransactionEventData::Type::Ended :
                                         TransactionEventData::Type::Updated;
        
        txEvent->timestamp = context.getModel().getClock().now();
        txEvent->triggerReason = triggerReason;
        
        if (chargingState != trackChargingState) {
            txEvent->chargingState = chargingState;
        }
        trackChargingState = chargingState;

        if (txStopped) {
            txEvent->stoppedReason = stoppedReason;
        }

        if (authorization.get() && !transaction->idTokenTransmitted) {
            txEvent->transaction->idToken = authorization;
            txEvent->idTokenTransmit = true;
            transaction->idTokenTransmitted = true;
        }

        txEvent->evse = evseId;

        // meterValue not supported

        auto txEventRequest = makeRequest(new Ocpp201::TransactionEvent(context.getModel(), txEvent));
        txEventRequest->setTimeout(0);
        context.initiateRequest(std::move(txEventRequest));

        if (txStopped) {
            MO_DBG_DEBUG("drop completed transaction");
            transaction.reset();
        }
    }
}

void TransactionService::Evse::setConnectorPluggedInput(std::function<bool()> connectorPlugged) {
    this->connectorPluggedInput = connectorPlugged;
}

void TransactionService::Evse::setEvReadyInput(std::function<bool()> evRequestsEnergy) {
    this->evReadyInput = evRequestsEnergy;
}

void TransactionService::Evse::setEvseReadyInput(std::function<bool()> connectorEnergized) {
    this->evseReadyInput = connectorEnergized;
}

bool TransactionService::Evse::beginAuthorization(IdToken idToken) {
    MO_DBG_DEBUG("begin auth: %s", idToken.get());
    IdToken idTokenCpy = idToken;
    auto authorize = makeRequest(new Ocpp16::CustomOperation(
            "Authorize",
            [idTokenCpy] () {
                auto doc = std::unique_ptr<DynamicJsonDocument>(new DynamicJsonDocument(
                        JSON_OBJECT_SIZE(3) +
                        JSON_OBJECT_SIZE(2) +
                        MO_IDTOKEN_LEN_MAX + 1));
                auto payload = doc->to<JsonObject>();
                payload["idToken"]["idToken"] = idTokenCpy.get();
                payload["idToken"]["type"] = idTokenCpy.getTypeCstr();
                return doc;
            }, [this, idTokenCpy] (JsonObject response) {
                if (!strcmp(response["idTokenInfo"]["status"], "Accepted")) {
                    authorization = idTokenCpy;
                    if (transaction) {
                        transaction->idTokenTransmitted = false;
                    }
                    MO_DBG_DEBUG("confirmed auth: %s", idTokenCpy.get());
                }
            }));
    context.initiateRequest(std::move(authorize));
    return true;
}
bool TransactionService::Evse::endAuthorization(IdToken idToken) {
    MO_DBG_DEBUG("end auth: %s", idToken.get()? idToken.get() : "(empty)");
    authorization = IdToken();
    if (transaction) {
        transaction->idTokenTransmitted = false;
    }
    return true;
}
const char *TransactionService::Evse::getAuthorization() {
    return authorization.get();
}

bool TransactionService::isTxStartPoint(TxStartStopPoint check) {
    for (auto& v : txStartPointParsed) {
        if (v == check) {
            return true;
        }
    }
    return false;
}
bool TransactionService::isTxStopPoint(TxStartStopPoint check) {
    for (auto& v : txStopPointParsed) {
        if (v == check) {
            return true;
        }
    }
    return false;
}

bool TransactionService::parseTxStartStopPoint(const char *csl, std::vector<TxStartStopPoint>& dst) {
    dst.clear();

    while (*csl == ',') {
        csl++;
    }

    while (*csl) {
        if (!strncmp(csl, "ParkingBayOccupancy", sizeof("ParkingBayOccupancy") - 1)
                && (csl[sizeof("ParkingBayOccupancy") - 1] == '\0' || csl[sizeof("ParkingBayOccupancy") - 1] == ',')) {
            dst.push_back(TxStartStopPoint::ParkingBayOccupancy);
            csl += sizeof("ParkingBayOccupancy") - 1;
        } else if (!strncmp(csl, "EVConnected", sizeof("EVConnected") - 1)
                && (csl[sizeof("EVConnected") - 1] == '\0' || csl[sizeof("EVConnected") - 1] == ',')) {
            dst.push_back(TxStartStopPoint::EVConnected);
            csl += sizeof("EVConnected") - 1;
        } else if (!strncmp(csl, "Authorized", sizeof("Authorized") - 1)
                && (csl[sizeof("Authorized") - 1] == '\0' || csl[sizeof("Authorized") - 1] == ',')) {
            dst.push_back(TxStartStopPoint::Authorized);
            csl += sizeof("Authorized") - 1;
        } else if (!strncmp(csl, "DataSigned", sizeof("DataSigned") - 1)
                && (csl[sizeof("DataSigned") - 1] == '\0' || csl[sizeof("DataSigned") - 1] == ',')) {
            dst.push_back(TxStartStopPoint::DataSigned);
            csl += sizeof("DataSigned") - 1;
        } else if (!strncmp(csl, "PowerPathClosed", sizeof("PowerPathClosed") - 1)
                && (csl[sizeof("PowerPathClosed") - 1] == '\0' || csl[sizeof("PowerPathClosed") - 1] == ',')) {
            dst.push_back(TxStartStopPoint::PowerPathClosed);
            csl += sizeof("PowerPathClosed") - 1;
        } else if (!strncmp(csl, "EnergyTransfer", sizeof("EnergyTransfer") - 1)
                && (csl[sizeof("EnergyTransfer") - 1] == '\0' || csl[sizeof("EnergyTransfer") - 1] == ',')) {
            dst.push_back(TxStartStopPoint::EnergyTransfer);
            csl += sizeof("EnergyTransfer") - 1;
        } else {
            MO_DBG_ERR("unkown TxStartStopPoint");
            dst.clear();
            return false;
        }

        while (*csl == ',') {
            csl++;
        }
    }

    return true;
}

TransactionService::TransactionService(Context& context) : context(context) {
    auto variableService = context.getModel().getVariableService();

    TxStartPointString = variableService->declareVariable<const char*>("TxCtrlr", "TxStartPoint", "PowerPathClosed");
    TxStopPointString  = variableService->declareVariable<const char*>("TxCtrlr", "TxStopPoint",  "Authorized,EVConnected");

    variableService->registerValidator<const char*>("TxCtrlr", "TxStartPoint", [this] (const char *value) -> bool {
        std::vector<TxStartStopPoint> validated;
        return this->parseTxStartStopPoint(value, validated);
    });

    variableService->registerValidator<const char*>("TxCtrlr", "TxStopPoint", [this] (const char *value) -> bool {
        std::vector<TxStartStopPoint> validated;
        return this->parseTxStartStopPoint(value, validated);
    });

    for (unsigned int evseId = 1; evseId < MO_NUM_EVSE; evseId++) {
        evses.emplace_back(context, *this, evseId);
    }
}

void TransactionService::loop() {
    for (Evse& evse : evses) {
        evse.loop();
    }

    if (TxStartPointString->getWriteCount() != trackTxStartPoint) {
        parseTxStartStopPoint(TxStartPointString->getString(), txStartPointParsed);
    }

    if (TxStopPointString->getWriteCount() != trackTxStopPoint) {
        parseTxStartStopPoint(TxStopPointString->getString(), txStopPointParsed);
    }
}

TransactionService::Evse *TransactionService::getEvse(unsigned int evseId) {
    if (evseId >= 1 && evseId - 1 < evses.size()) {
        return &evses[evseId - 1];
    } else {
        MO_DBG_ERR("invalid arg");
        return nullptr;
    }
}

#endif // MO_ENABLE_V201
