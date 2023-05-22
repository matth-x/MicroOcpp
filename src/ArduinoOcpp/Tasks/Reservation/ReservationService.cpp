// matth-x/ArduinoOcpp
// Copyright Matthias Akstaller 2019 - 2023
// MIT License

#include <ArduinoOcpp/Tasks/Reservation/ReservationService.h>
#include <ArduinoOcpp/Core/OcppEngine.h>
#include <ArduinoOcpp/Core/OcppModel.h>
#include <ArduinoOcpp/Tasks/ChargeControl/Connector.h>
#include <ArduinoOcpp/Tasks/Transactions/Transaction.h>
#include <ArduinoOcpp/MessagesV16/CancelReservation.h>
#include <ArduinoOcpp/MessagesV16/ReserveNow.h>

#include <ArduinoOcpp/Debug.h>

using namespace ArduinoOcpp;

ReservationService::ReservationService(OcppEngine& context, unsigned int numConnectors) : context(context), maxReservations((int) numConnectors - 1) {
    if (maxReservations > 0) {
        reservations.reserve((size_t) maxReservations);
        for (int i = 0; i < maxReservations; i++) {
            reservations.push_back(Reservation(context.getOcppModel(), i));
        }
    }

    reserveConnectorZeroSupported = declareConfiguration<bool>("ReserveConnectorZeroSupported", true, CONFIGURATION_VOLATILE, false, true, false, false);

    //append "Reservation" to FeatureProfiles list
    const char *fpId = "Reservation";
    auto fProfile = declareConfiguration<const char*>("SupportedFeatureProfiles",fpId, CONFIGURATION_VOLATILE, false, true, true, false);
    if (!strstr(*fProfile, fpId)) {
        auto fProfilePlus = std::string(*fProfile);
        if (!fProfilePlus.empty() && fProfilePlus.back() != ',')
            fProfilePlus += ",";
        fProfilePlus += fpId;
        fProfile->setValue(fProfilePlus.c_str(), fProfilePlus.length() + 1);
    }
    
    context.getOperationDeserializer().registerOcppOperation("CancelReservation", [&context] () {
        return new Ocpp16::CancelReservation(context.getOcppModel());});
    context.getOperationDeserializer().registerOcppOperation("ReserveNow", [&context] () {
        return new Ocpp16::ReserveNow(context.getOcppModel());});
}

void ReservationService::loop() {
    //check if to end reservations

    for (Reservation& reservation : reservations) {
        if (!reservation.isActive()) {
            continue;
        }

        if (auto connector = context.getOcppModel().getConnector(reservation.getConnectorId())) {

            //check if connector went inoperative
            auto cStatus = connector->inferenceStatus();
            if (cStatus == OcppEvseState::Faulted || cStatus == OcppEvseState::Unavailable) {
                reservation.clear();
                continue;
            }

            //check if other tx started at this connector (e.g. due to RemoteStartTransaction)
            if (connector->getSessionIdTag()) {
                reservation.clear();
                continue;
            }
        }

        //check if tx with same idTag or reservationId has started
        for (unsigned int cId = 1; cId < context.getOcppModel().getNumConnectors(); cId++) {
            auto& transaction = context.getOcppModel().getConnector(cId)->getTransaction();
            if (transaction) {
                const char *cIdTag = transaction->getIdTag();
                if (transaction->getReservationId() == reservation.getReservationId() || 
                        (cIdTag && !strcmp(cIdTag, reservation.getIdTag()))) {
                    
                    reservation.clear();
                    break;
                }
            }
        }
    }
}

Reservation *ReservationService::getReservation(unsigned int connectorId) {
    if (connectorId == 0) {
        AO_DBG_DEBUG("tried to fetch connectorId 0");
        return nullptr; //cannot fetch for connectorId 0 because multiple reservations are possible at a time
    }

    for (Reservation& reservation : reservations) {
        if (reservation.isActive() && reservation.matches(connectorId)) {
            return &reservation;
        }
    }
    
    return nullptr;
}

Reservation *ReservationService::getReservation(const char *idTag, const char *parentIdTag) {
    if (idTag == nullptr) {
        AO_DBG_ERR("invalid input");
        return nullptr;
    }

    Reservation *connectorReservation = nullptr;

    for (Reservation& reservation : reservations) {
        if (!reservation.isActive()) {
            continue;
        }

        //TODO check for parentIdTag

        if (reservation.matches(idTag, parentIdTag)) {
            if (reservation.getConnectorId() == 0) {
                return &reservation; //reservation at connectorId 0 has higher priority
            } else {
                connectorReservation = &reservation;
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

    if (reserveConnectorZeroSupported && !*reserveConnectorZeroSupported) {
        //no connectorZero check - all done
        AO_DBG_DEBUG("no reservation");
        return nullptr;
    }

    //connectorZero check
    Reservation *blockingReservation = nullptr; //any reservation which blocks this connector now

    //Check if there are enough free connectors to satisfy all reservations at connectorId 0
    unsigned int unspecifiedReservations = 0;
    for (Reservation& ires : reservations) {
        if (ires.isActive() && ires.getConnectorId() == 0) {
            unspecifiedReservations++;
            blockingReservation = &ires;
        }
    }

    unsigned int availableCount = 0;
    for (unsigned int cId = 1; cId < context.getOcppModel().getNumConnectors(); cId++) {
        if (cId == connectorId) {
            //don't count this connector
            continue;
        }
        if (auto connector = context.getOcppModel().getConnector(cId)) {
            if (connector->inferenceStatus() == OcppEvseState::Available) {
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
    for (Reservation& reservation : reservations) {
        if (reservation.isActive() && reservation.getReservationId() == reservationId) {
            return &reservation;
        }
    }

    return nullptr;
}

bool ReservationService::updateReservation(JsonObject payload) {
    if (auto reservation = getReservationById(payload["reservationId"])) {
        reservation->update(payload);
        return true;
    }

// Alternative condition: avoids that one idTag can make two reservations at a time. The specification doesn't
// mention that double-reservations should be possible but it seems to mean it. 
    if (auto reservation = getReservation(payload["connectorId"], nullptr, nullptr)) {
//                payload["idTag"],
//                payload.containsKey("parentIdTag") ? payload["parentIdTag"] : nullptr)) {
//    if (auto reservation = getReservation(payload["connectorId"].as<int>())) {
        AO_DBG_DEBUG("found blocking reservation at connectorId %u", reservation->getConnectorId());
        (void)reservation;
        return false;
    }

    //update free reservation slot
    for (Reservation& reservation : reservations) {
        if (!reservation.isActive()) {
            reservation.update(payload);
            return true;
        }
    }

    AO_DBG_ERR("error finding blocking reservation");
    return false;
}
