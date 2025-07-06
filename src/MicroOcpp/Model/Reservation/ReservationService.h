// matth-x/MicroOcpp
// Copyright Matthias Akstaller 2019 - 2024
// MIT License

#ifndef MO_RESERVATIONSERVICE_H
#define MO_RESERVATIONSERVICE_H

#include <memory>

#include <MicroOcpp/Model/Reservation/Reservation.h>
#include <MicroOcpp/Model/Common/EvseId.h>
#include <MicroOcpp/Core/Memory.h>
#include <MicroOcpp/Version.h>

#if MO_ENABLE_V16 && MO_ENABLE_RESERVATION

namespace MicroOcpp {

class Context;

namespace v16 {

class ReservationService : public MemoryManaged {
private:
    Context& context;

    Reservation *reservations [MO_NUM_EVSEID - 1] = {nullptr}; // = number of physical connectors
    size_t maxReservations = MO_NUM_EVSEID - 1;

    unsigned int numEvseId = MO_NUM_EVSEID;

    Configuration *reserveConnectorZeroSupportedBool = nullptr;

public:
    ReservationService(Context& context);
    ~ReservationService();

    bool setup();

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

} //namespace v16
} //namespace MicroOcpp
#endif //MO_ENABLE_V16 && MO_ENABLE_RESERVATION
#endif
