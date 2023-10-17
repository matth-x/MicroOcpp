// matth-x/MicroOcpp
// Copyright Matthias Akstaller 2019 - 2023
// MIT License

#ifndef RESERVATION_H
#define RESERVATION_H

#include <MicroOcpp/Core/Configuration.h>
#include <MicroOcpp/Core/Time.h>

#ifndef RESERVATION_FN
#define RESERVATION_FN (MO_FILENAME_PREFIX "reservations.jsn")
#endif

#define MO_RESERVATION_CID_KEY "cid_"
#define MO_RESERVATION_EXPDATE_KEY "expdt_"
#define MO_RESERVATION_IDTAG_KEY "idt_"
#define MO_RESERVATION_RESID_KEY "rsvid_"
#define MO_RESERVATION_PARENTID_KEY "pidt_"

namespace MicroOcpp {

class Model;

class Reservation {
private:
    Model& model;
    const unsigned int slot;

    std::shared_ptr<Configuration> connectorIdInt;
    char connectorIdKey [sizeof(MO_RESERVATION_CID_KEY "xxx") + 1]; //"xxx" = placeholder for digits
    std::shared_ptr<Configuration> expiryDateRawString;
    char expiryDateRawKey [sizeof(MO_RESERVATION_EXPDATE_KEY "xxx") + 1];

    Timestamp expiryDate = MIN_TIME;
    std::shared_ptr<Configuration> idTagString;
    char idTagKey [sizeof(MO_RESERVATION_IDTAG_KEY "xxx") + 1];
    std::shared_ptr<Configuration> reservationIdInt;
    char reservationIdKey [sizeof(MO_RESERVATION_RESID_KEY "xxx") + 1];
    std::shared_ptr<Configuration> parentIdTagString;
    char parentIdTagKey [sizeof(MO_RESERVATION_PARENTID_KEY "xxx") + 1];

public:
    Reservation(Model& model, unsigned int slot);
    Reservation(const Reservation&) = delete;
    Reservation(Reservation&&) = delete;
    Reservation& operator=(const Reservation&) = delete;

    ~Reservation();

    bool isActive(); //if this object contains a valid, unexpired reservation

    bool matches(unsigned int connectorId);
    bool matches(const char *idTag, const char *parentIdTag = nullptr); //idTag == parentIdTag == nullptr -> return True

    int getConnectorId();
    Timestamp& getExpiryDate();
    const char *getIdTag();
    int getReservationId();
    const char *getParentIdTag();

    void update(int reservationId, unsigned int connectorId, Timestamp expiryDate, const char *idTag, const char *parentIdTag = nullptr);
    void clear();
};

}

#endif
