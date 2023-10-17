// matth-x/MicroOcpp
// Copyright Matthias Akstaller 2019 - 2023
// MIT License

#include <MicroOcpp/Model/Reservation/ReservationService.h>
#include <MicroOcpp/Core/Context.h>
#include <MicroOcpp/Model/Model.h>
#include <MicroOcpp/Model/ConnectorBase/Connector.h>
#include <MicroOcpp/Model/Transactions/Transaction.h>
#include <MicroOcpp/Operations/CancelReservation.h>
#include <MicroOcpp/Operations/ReserveNow.h>

#include <MicroOcpp/Debug.h>

using namespace MicroOcpp;

ReservationService::ReservationService(Context& context, unsigned int numConnectors) : context(context), maxReservations((int) numConnectors - 1) {
    if (maxReservations > 0) {
        reservations.reserve((size_t) maxReservations);
        for (int i = 0; i < maxReservations; i++) {
            reservations.emplace_back(new Reservation(context.getModel(), i));
        }
    }

    reserveConnectorZeroSupportedBool = declareConfiguration<bool>("ReserveConnectorZeroSupported", true, CONFIGURATION_VOLATILE, true);
    
    context.getOperationRegistry().registerOperation("CancelReservation", [&context] () {
        return new Ocpp16::CancelReservation(context.getModel());});
    context.getOperationRegistry().registerOperation("ReserveNow", [&context] () {
        return new Ocpp16::ReserveNow(context.getModel());});
}

void ReservationService::loop() {
    //check if to end reservations

    for (auto& reservation : reservations) {
        if (!reservation->isActive()) {
            continue;
        }

        if (auto connector = context.getModel().getConnector(reservation->getConnectorId())) {

            //check if connector went inoperative
            auto cStatus = connector->getStatus();
            if (cStatus == ChargePointStatus::Faulted || cStatus == ChargePointStatus::Unavailable) {
                reservation->clear();
                continue;
            }

            //check if other tx started at this connector (e.g. due to RemoteStartTransaction)
            if (connector->getTransaction() && connector->getTransaction()->isAuthorized()) {
                reservation->clear();
                continue;
            }
        }

        //check if tx with same idTag or reservationId has started
        for (unsigned int cId = 1; cId < context.getModel().getNumConnectors(); cId++) {
            auto& transaction = context.getModel().getConnector(cId)->getTransaction();
            if (transaction && transaction->isAuthorized()) {
                const char *cIdTag = transaction->getIdTag();
                if (transaction->getReservationId() == reservation->getReservationId() || 
                        (cIdTag && !strcmp(cIdTag, reservation->getIdTag()))) {

                    reservation->clear();
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

    for (auto& reservation : reservations) {
        if (reservation->isActive() && reservation->matches(connectorId)) {
            return reservation.get();
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

    for (auto& reservation : reservations) {
        if (!reservation->isActive()) {
            continue;
        }

        //TODO check for parentIdTag

        if (reservation->matches(idTag, parentIdTag)) {
            if (reservation->getConnectorId() == 0) {
                return reservation.get(); //reservation at connectorId 0 has higher priority
            } else {
                connectorReservation = reservation.get();
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

    if (reserveConnectorZeroSupportedBool && !reserveConnectorZeroSupportedBool->getBool()) {
        //no connectorZero check - all done
        MO_DBG_DEBUG("no reservation");
        return nullptr;
    }

    //connectorZero check
    Reservation *blockingReservation = nullptr; //any reservation which blocks this connector now

    //Check if there are enough free connectors to satisfy all reservations at connectorId 0
    unsigned int unspecifiedReservations = 0;
    for (auto& reservation : reservations) {
        if (reservation->isActive() && reservation->getConnectorId() == 0) {
            unspecifiedReservations++;
            blockingReservation = reservation.get();
        }
    }

    unsigned int availableCount = 0;
    for (unsigned int cId = 1; cId < context.getModel().getNumConnectors(); cId++) {
        if (cId == connectorId) {
            //don't count this connector
            continue;
        }
        if (auto connector = context.getModel().getConnector(cId)) {
            if (connector->getStatus() == ChargePointStatus::Available) {
                availableCount++;
            }
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
    for (auto& reservation : reservations) {
        if (reservation->isActive() && reservation->getReservationId() == reservationId) {
            return reservation.get();
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
    for (auto& reservation : reservations) {
        if (!reservation->isActive()) {
            reservation->update(reservationId, connectorId, expiryDate, idTag, parentIdTag);
            return true;
        }
    }

    MO_DBG_ERR("error finding blocking reservation");
    return false;
}
