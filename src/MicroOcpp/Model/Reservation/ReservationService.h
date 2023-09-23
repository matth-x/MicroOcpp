// matth-x/MicroOcpp
// Copyright Matthias Akstaller 2019 - 2023
// MIT License

#ifndef RESERVATIONSERVICE_H
#define RESERVATIONSERVICE_H

#include <MicroOcpp/Model/Reservation/Reservation.h>

#include <memory>

namespace MicroOcpp {

class Context;

class ReservationService {
private:
    Context& context;

    const int maxReservations; // = number of physical connectors
    std::vector<std::unique_ptr<Reservation>> reservations;

    std::shared_ptr<Configuration> reserveConnectorZeroSupportedBool;

public:
    ReservationService(Context& context, unsigned int numConnectors);

    void loop();

    Reservation *getReservation(unsigned int connectorId); //by connectorId
    Reservation *getReservation(const char *idTag, const char *parentIdTag = nullptr); //by idTag

    /*
     * Get prevailing reservation for a charging session authorized by idTag/parentIdTag at connectorId.
     * returns nullptr if there is no reservation in question
     * returns a reservation if applicable. Caller must check if idTag/parentIdTag match before starting a transaction
     */
    Reservation *getReservation(unsigned int connectorId, const char *idTag, const char *parentIdtag = nullptr);

    Reservation *getReservationById(int reservationId);

    bool updateReservation(int reservationId, unsigned int connectorId, Timestamp expiryDate, const char *idTag, const char *parentIdTag = nullptr);
};

}

#endif
