// matth-x/MicroOcpp
// Copyright Matthias Akstaller 2019 - 2024
// MIT License

#include <MicroOcpp/Model/Reservation/ReservationService.h>

#include <MicroOcpp/Context.h>
#include <MicroOcpp/Model/Model.h>
#include <MicroOcpp/Model/Availability/AvailabilityService.h>
#include <MicroOcpp/Model/Configuration/ConfigurationService.h>
#include <MicroOcpp/Model/Transactions/TransactionService16.h>
#include <MicroOcpp/Model/Transactions/TransactionService201.h>
#include <MicroOcpp/Operations/CancelReservation.h>
#include <MicroOcpp/Operations/ReserveNow.h>
#include <MicroOcpp/Debug.h>

#if MO_ENABLE_V16 && MO_ENABLE_RESERVATION

using namespace MicroOcpp;
using namespace MicroOcpp::Ocpp16;

ReservationService::ReservationService(Context& context) : MemoryManaged("v16.Reservation.ReservationService"), context(context) {

}

ReservationService::~ReservationService() {
    for (size_t i = 0; i < sizeof(reservations) / sizeof(reservations[0]); i++) {
        delete reservations[i];
        reservations[i] = nullptr;
    }
}

bool ReservationService::setup() {

    numEvseId = context.getModel16().getNumEvseId();

    maxReservations = numEvseId > 0 ? numEvseId - 1 : 0; // = number of physical connectors
    for (size_t i = 0; i < maxReservations; i++) {
        reservations[i] = new Reservation(context, i);
        if (!reservations[i] || !reservations[i]->setup()) {
            MO_DBG_ERR("OOM");
            return false;
        }
    }

    auto configService = context.getModel16().getConfigurationService();
    if (!configService) {
        MO_DBG_ERR("setup failure");
        return false;
    }

    reserveConnectorZeroSupportedBool = configService->declareConfiguration<bool>("ReserveConnectorZeroSupported", true, MO_CONFIGURATION_VOLATILE, Mutability::ReadOnly);
    if (!reserveConnectorZeroSupportedBool) {
        MO_DBG_ERR("declareConfiguration failed");
        return false;
    }

    context.getMessageService().registerOperation("CancelReservation", [] (Context& context) -> Operation* {
        return new CancelReservation(*context.getModel16().getReservationService());});
    context.getMessageService().registerOperation("ReserveNow", [] (Context& context) -> Operation* {
        return new ReserveNow(context, *context.getModel16().getReservationService());});

    return true;
}

void ReservationService::loop() {
    //check if to end reservations

    for (size_t i = 0; i < maxReservations; i++) {
        if (!reservations[i]->isActive()) {
            continue;
        }

        auto availSvc = context.getModel16().getAvailabilityService();
        auto availSvcEvse = availSvc ? availSvc->getEvse(reservations[i]->getConnectorId()) : nullptr;
        if (availSvcEvse) {
            //check if connector went inoperative
            auto cStatus = availSvcEvse->getStatus();
            if (cStatus == MO_ChargePointStatus_Faulted || cStatus == MO_ChargePointStatus_Unavailable) {
                reservations[i]->clear();
                continue;
            }
        }

        auto txSvc = context.getModel16().getTransactionService();
        auto txSvcEvse = txSvc ? txSvc->getEvse(reservations[i]->getConnectorId()) : nullptr;
        if (txSvcEvse) {
            //check if other tx started at this connector (e.g. due to RemoteStartTransaction)
            if (txSvcEvse->getTransaction() && txSvcEvse->getTransaction()->isAuthorized()) {
                reservations[i]->clear();
                continue;
            }
        }

        //check if tx with same idTag or reservationId has started
        for (unsigned int evseId = 1; evseId < numEvseId; evseId++) {
            auto txSvc = context.getModel16().getTransactionService();
            auto txSvcEvse = txSvc ? txSvc->getEvse(evseId) : nullptr;
            auto transaction = txSvcEvse ? txSvcEvse->getTransaction() : nullptr;
            if (transaction && transaction->isAuthorized()) {
                const char *idTag = transaction->getIdTag();
                if (transaction->getReservationId() == reservations[i]->getReservationId() || 
                        (idTag && !strcmp(idTag, reservations[i]->getIdTag()))) {

                    reservations[i]->clear();
                    break;
                }
            }
        }
    }
}

Reservation *ReservationService::getReservation(unsigned int connectorId) {
    if (connectorId == 0) {
        MO_DBG_DEBUG("tried to fetch connectorId 0");
        return nullptr; //cannot fetch for connectorId 0 because multiple reservations are possible at a time
    }

    for (size_t i = 0; i < maxReservations; i++) {
        if (reservations[i]->isActive() && reservations[i]->matches(connectorId)) {
            return reservations[i];
        }
    }
    
    return nullptr;
}

Reservation *ReservationService::getReservation(const char *idTag, const char *parentIdTag) {
    if (idTag == nullptr) {
        MO_DBG_ERR("invalid input");
        return nullptr;
    }

    Reservation *connectorReservation = nullptr;

    for (size_t i = 0; i < maxReservations; i++) {
        if (!reservations[i]->isActive()) {
            continue;
        }

        //TODO check for parentIdTag

        if (reservations[i]->matches(idTag, parentIdTag)) {
            if (reservations[i]->getConnectorId() == 0) {
                return reservations[i]; //reservation at connectorId 0 has higher priority
            } else {
                connectorReservation = reservations[i];
            }
        }
    }

    return connectorReservation;
}

Reservation *ReservationService::getReservation(unsigned int connectorId, const char *idTag, const char *parentIdTag) {

    //is connector blocked by a reservation?
    if (auto reservation = getReservation(connectorId)) {
        //connector has reservation -> will always be the prevailing reservation
        return reservation;
    }

    //is there any reservation at this charge point for idTag?
    if (idTag) {
        if (auto reservation = getReservation(idTag, parentIdTag)) {
            //yes, can use reservation with different connectorId
            return reservation;
        }
    }

    if (!reserveConnectorZeroSupportedBool->getBool()) {
        //no connectorZero check - all done
        MO_DBG_DEBUG("no reservation");
        return nullptr;
    }

    //connectorZero check
    Reservation *blockingReservation = nullptr; //any reservation which blocks this connector now

    //Check if there are enough free connectors to satisfy all reservations at connectorId 0
    unsigned int unspecifiedReservations = 0;
    for (size_t i = 0; i < maxReservations; i++) {
        if (reservations[i]->isActive() && reservations[i]->getConnectorId() == 0) {
            unspecifiedReservations++;
            blockingReservation = reservations[i];
        }
    }

    unsigned int availableCount = 0;
    for (unsigned int eId = 1; eId < numEvseId; eId++) {
        if (eId == connectorId) {
            //don't count this connector
            continue;
        }
        auto availSvc = context.getModel16().getAvailabilityService();
        auto availSvcEvse = availSvc ? availSvc->getEvse(eId) : nullptr;
        if (availSvcEvse && availSvcEvse->getStatus() == MO_ChargePointStatus_Available) {
            availableCount++;
        }
    }

    if (availableCount >= unspecifiedReservations) {
        //enough other connectors available to satisfy all reservations
        return nullptr;
    } else {
        //not sufficient connectors for all reservations after this action
        return blockingReservation;
    }
}

Reservation *ReservationService::getReservationById(int reservationId) {
    for (size_t i = 0; i < maxReservations; i++) {
        if (reservations[i]->isActive() && reservations[i]->getReservationId() == reservationId) {
            return reservations[i];
        }
    }

    return nullptr;
}

bool ReservationService::updateReservation(int reservationId, unsigned int connectorId, Timestamp expiryDate, const char *idTag, const char *parentIdTag) {
    if (auto reservation = getReservationById(reservationId)) {
        if (getReservation(connectorId) && getReservation(connectorId) != reservation && getReservation(connectorId)->isActive()) {
            MO_DBG_DEBUG("found blocking reservation at connectorId %u", connectorId);
            return false; //cannot transfer reservation to other connector with existing reservation
        }
        reservation->update(reservationId, connectorId, expiryDate, idTag, parentIdTag);
        return true;
    }

// Alternative condition: avoids that one idTag can make two reservations at a time. The specification doesn't
// mention that double-reservations should be possible but it seems to mean it. 
    if (auto reservation = getReservation(connectorId, nullptr, nullptr)) {
//                payload["idTag"],
//                payload.containsKey("parentIdTag") ? payload["parentIdTag"] : nullptr)) {
//    if (auto reservation = getReservation(payload["connectorId"].as<int>())) {
        MO_DBG_DEBUG("found blocking reservation at connectorId %u", reservation->getConnectorId());
        (void)reservation;
        return false;
    }

    //update free reservation slot
    for (size_t i = 0; i < maxReservations; i++) {
        if (!reservations[i]->isActive()) {
            reservations[i]->update(reservationId, connectorId, expiryDate, idTag, parentIdTag);
            return true;
        }
    }

    MO_DBG_ERR("error finding blocking reservation");
    return false;
}

#endif //MO_ENABLE_V16 && MO_ENABLE_RESERVATION
