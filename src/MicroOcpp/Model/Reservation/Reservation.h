// matth-x/MicroOcpp
// Copyright Matthias Akstaller 2019 - 2024
// MIT License

#ifndef MO_RESERVATION_H
#define MO_RESERVATION_H

#include <MicroOcpp/Core/Time.h>
#include <MicroOcpp/Core/Memory.h>
#include <MicroOcpp/Version.h>

#ifndef RESERVATION_FN
#define RESERVATION_FN "reservations.jsn"
#endif

#define MO_RESERVATION_CID_KEY "cid_"
#define MO_RESERVATION_EXPDATE_KEY "expdt_"
#define MO_RESERVATION_IDTAG_KEY "idt_"
#define MO_RESERVATION_RESID_KEY "rsvid_"
#define MO_RESERVATION_PARENTID_KEY "pidt_"

#if MO_ENABLE_V16 && MO_ENABLE_RESERVATION

namespace MicroOcpp {

class Context;

namespace Ocpp16 {

class Configuration;
class ConfigurationContainer;

class Reservation : public MemoryManaged {
private:
    Context& context;
    const unsigned int slot;

    ConfigurationContainer *reservations = nullptr;

    Configuration *connectorIdInt = nullptr;
    char connectorIdKey [sizeof(MO_RESERVATION_CID_KEY "xxx") + 1]; //"xxx" = placeholder for digits
    Configuration *expiryDateRawString = nullptr;
    char expiryDateRawKey [sizeof(MO_RESERVATION_EXPDATE_KEY "xxx") + 1];

    Timestamp expiryDate;
    Configuration *idTagString = nullptr;
    char idTagKey [sizeof(MO_RESERVATION_IDTAG_KEY "xxx") + 1];
    Configuration *reservationIdInt = nullptr;
    char reservationIdKey [sizeof(MO_RESERVATION_RESID_KEY "xxx") + 1];
    Configuration *parentIdTagString = nullptr;
    char parentIdTagKey [sizeof(MO_RESERVATION_PARENTID_KEY "xxx") + 1];

public:
    Reservation(Context& context, unsigned int slot);
    Reservation(const Reservation&) = delete;
    Reservation(Reservation&&) = delete;
    Reservation& operator=(const Reservation&) = delete;

    ~Reservation();

    bool setup();

    bool isActive(); //if this object contains a valid, unexpired reservation

    bool matches(unsigned int connectorId);
    bool matches(const char *idTag, const char *parentIdTag = nullptr); //idTag == parentIdTag == nullptr -> return True

    int getConnectorId();
    Timestamp& getExpiryDate();
    const char *getIdTag();
    int getReservationId();
    const char *getParentIdTag();

    bool update(int reservationId, unsigned int connectorId, Timestamp expiryDate, const char *idTag, const char *parentIdTag = nullptr);
    bool clear();
};

} //namespace Ocpp16
} //namespace MicroOcpp
#endif //MO_ENABLE_V16 && MO_ENABLE_RESERVATION
#endif
