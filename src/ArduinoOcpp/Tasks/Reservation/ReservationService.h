// matth-x/ArduinoOcpp
// Copyright Matthias Akstaller 2019 - 2023
// MIT License

#ifndef RESERVATIONSERVICE_H
#define RESERVATIONSERVICE_H

#include <ArduinoOcpp/Tasks/Reservation/Reservation.h>

namespace ArduinoOcpp {

class OcppEngine;

class ReservationService {
private:
    OcppEngine& context;

    const int maxReservations; // = number of physical connectors
    std::vector<Reservation> reservations;

    std::shared_ptr<Configuration<bool>> reserveConnectorZeroSupported;

public:
    ReservationService(OcppEngine& context, unsigned int numConnectors);

    Reservation *getReservation(unsigned int connectorId); //by connectorId
    Reservation *getReservation(const char *idTag, const char *parentIdTag = nullptr); //by idTag

    /*
     * Get prevailing reservation for a charging session authorized by idTag/parentIdTag at connectorId.
     * returns nullptr if there is no reservation in question
     * returns a reservation if applicable. Caller must check if idTag/parentIdTag match before starting a transaction
     */
    Reservation *getReservation(unsigned int connectorId, const char *idTag, const char *parentIdtag = nullptr);

    Reservation *getReservationById(int reservationId);

    bool updateReservation(JsonObject payload);
};

}

#endif
